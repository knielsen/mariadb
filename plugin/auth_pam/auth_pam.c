#include <mysql/plugin_auth.h>
#include <string.h>
#include <my_config.h>
#include <security/pam_appl.h>
#include <security/pam_modules.h>

struct param {
  unsigned char buf[10240], *ptr;
  MYSQL_PLUGIN_VIO *vio;
};

/* It least solaris doesn't have strndup */

#ifndef HAVE_STRNDUP
char *strndup(const char *from, size_t length)
{
  char *ptr;
  size_t max_length= strlen(from);
  if (length > max_length)
    length= max_length;
  if ((ptr= (char*) malloc(length+1)) != 0)
  {
    memcpy((char*) ptr, (char*) from, length);
    ptr[length]=0;
  }
  return ptr;
}
#endif

static int conv(int n, const struct pam_message **msg,
                struct pam_response **resp, void *data)
{
  struct param *param = (struct param *)data;
  unsigned char *end = param->buf + sizeof(param->buf) - 1;
  int i;

  *resp = 0;

  for (i = 0; i < n; i++)
  {
    /* if there's a message - append it to the buffer */
    if (msg[i]->msg)
    {
      int len = strlen(msg[i]->msg);
      if (len > end - param->ptr)
        len = end - param->ptr;
      if (len > 0)
      {
        memcpy(param->ptr, msg[i]->msg, len);
        param->ptr+= len;
        *(param->ptr)++ = '\n';
      }
    }
    /* if the message style is *_PROMPT_*, meaning PAM asks a question,
       send the accumulated text to the client, read the reply */
    if (msg[i]->msg_style == PAM_PROMPT_ECHO_OFF ||
        msg[i]->msg_style == PAM_PROMPT_ECHO_ON)
    {
      int pkt_len;
      unsigned char *pkt;

      /* allocate the response array.
         freeing it is the responsibility of the caller */
      if (*resp == 0)
      {
        *resp = calloc(sizeof(struct pam_response), n);
        if (*resp == 0)
          return PAM_BUF_ERR;
      }

      /* dialog plugin interprets the first byte of the packet
         as the magic number.
           2 means "read the input with the echo enabled"
           4 means "password-like input, echo disabled"
         C'est la vie. */
      param->buf[0] = msg[i]->msg_style == PAM_PROMPT_ECHO_ON ? 2 : 4;
      if (param->vio->write_packet(param->vio, param->buf, param->ptr - param->buf - 1))
        return PAM_CONV_ERR;

      pkt_len = param->vio->read_packet(param->vio, &pkt);
      if (pkt_len < 0)
        return PAM_CONV_ERR;
      /* allocate and copy the reply to the response array */
      (*resp)[i].resp = strndup((char*)pkt, pkt_len);
      param->ptr = param->buf + 1;
    }
  }
  return PAM_SUCCESS;
}

#define DO(X) if ((status = (X)) != PAM_SUCCESS) goto end

#ifdef SOLARIS
typedef void** pam_get_item_3_arg;
#else
typedef const void** pam_get_item_3_arg;
#endif

static int pam_auth(MYSQL_PLUGIN_VIO *vio, MYSQL_SERVER_AUTH_INFO *info)
{
  pam_handle_t *pamh = NULL;
  int status;
  const char *new_username;
  struct param param;
  /* The following is written in such a way to make also solaris happy */
  struct pam_conv pam_start_arg = { &conv, NULL };
  pam_start_arg.appdata_ptr= (char*) &param;

  /*
    get the service name, as specified in

     CREATE USER ... IDENTIFIED WITH pam_auth AS  "service"
  */
  const char *service = info->auth_string && info->auth_string[0]
                          ? info->auth_string : "mysql";

  param.ptr = param.buf + 1;
  param.vio = vio;

  DO( pam_start(service, info->user_name, &pam_start_arg, &pamh) );
  DO( pam_authenticate (pamh, 0) );
  DO( pam_acct_mgmt(pamh, 0) );
  DO( pam_get_item(pamh, PAM_USER, (pam_get_item_3_arg) &new_username) );

  if (new_username && strcmp(new_username, info->user_name))
    strncpy(info->authenticated_as, new_username,
            sizeof(info->authenticated_as));

end:
  pam_end(pamh, status);
  return status == PAM_SUCCESS ? CR_OK : CR_ERROR;
}

static struct st_mysql_auth pam_info =
{
  MYSQL_AUTHENTICATION_INTERFACE_VERSION,
  "dialog",
  pam_auth
};

maria_declare_plugin(pam)
{
  MYSQL_AUTHENTICATION_PLUGIN,
  &pam_info,
  "pam",
  "Sergei Golubchik",
  "PAM based authentication",
  PLUGIN_LICENSE_GPL,
  NULL,
  NULL,
  0x0100,
  NULL,
  NULL,
  "1.0",
  MariaDB_PLUGIN_MATURITY_BETA
}
maria_declare_plugin_end;
