#!/bin/sh
# Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
# This file is public domain and comes with NO WARRANTY of any kind
#
# Script to start the MySQL daemon and restart it if it dies unexpectedly
#
# This should be executed in the MySQL base directory if you are using a
# binary installation that is not installed in its compile-time default
# location
#
# mysql.server works by first doing a cd to the base directory and from there
# executing mysqld_safe

KILL_MYSQLD=1;
MYSQLD=
niceness=0
# Initial logging status: error log is not open, and not using syslog
logging=init
want_syslog=0
syslog_tag=
user='@MYSQLD_USER@'
pid_file=
err_log=

syslog_tag_mysqld=mysqld
syslog_tag_mysqld_safe=mysqld_safe

trap '' 1 2 3 15			# we shouldn't let anyone kill us

# MySQL-specific environment variable. First off, it's not really a umask,
# it's the desired mode. Second, it follows umask(2), not umask(3) in that
# octal needs to be explicit. Our shell might be a proper sh without printf,
# multiple-base arithmetic, and binary arithmetic, so this will get ugly.
# We reject decimal values to keep things at least half-sane.
umask 007                               # fallback
UMASK="${UMASK-0640}"
fmode=`echo "$UMASK" | sed -e 's/[^0246]//g'`
octalp=`echo "$fmode"|cut -c1`
fmlen=`echo "$fmode"|wc -c|sed -e 's/ //g'`
if [ "x$octalp" != "x0" -o "x$UMASK" != "x$fmode" -o "x$fmlen" != "x5" ]
then
  fmode=0640
  echo "UMASK must be a 3-digit mode with an additional leading 0 to indicate octal." >&2
  echo "The first digit will be corrected to 6, the others may be 0, 2, 4, or 6." >&2
fi
fmode=`echo "$fmode"|cut -c3-4`
fmode="6$fmode"
if [ "x$UMASK" != "x0$fmode" ]
then
  echo "UMASK corrected from $UMASK to 0$fmode ..."
fi

defaults=
case "$1" in
    --no-defaults|--defaults-file=*|--defaults-extra-file=*)
      defaults="$1"; shift
      ;;
esac

usage () {
        cat <<EOF
Usage: $0 [OPTIONS]
  --no-defaults              Don't read the system defaults file
  --defaults-file=FILE       Use the specified defaults file
  --defaults-extra-file=FILE Also use defaults from the specified file
  --ledir=DIRECTORY          Look for mysqld in the specified directory
  --open-files-limit=LIMIT   Limit the number of open files
  --core-file-size=LIMIT     Limit core files to the specified size
  --timezone=TZ              Set the system timezone
  --mysqld=FILE              Use the specified file as mysqld
  --mysqld-version=VERSION   Use "mysqld-VERSION" as mysqld
  --nice=NICE                Set the scheduling priority of mysqld
  --skip-kill-mysqld         Don't try to kill stray mysqld processes
  --syslog                   Log messages to syslog with 'logger'
  --skip-syslog              Log messages to error log (default)
  --syslog-tag=TAG           Pass -t "mysqld-TAG" to 'logger'

All other options are passed to the mysqld program.

EOF
        exit 1
}

my_which ()
{
  save_ifs="${IFS-UNSET}"
  IFS=:
  ret=0
  for file
  do
    for dir in $PATH
    do
      if [ -f "$dir/$file" ]
      then
        echo "$dir/$file"
        continue 2
      fi
    done

	ret=1  #signal an error
	break
  done

  if [ "$save_ifs" = UNSET ]
  then
    unset IFS
  else
    IFS="$save_ifs"
  fi
  return $ret  # Success
}

log_generic () {
  priority="$1"
  shift

  msg="`date +'%y%m%d %H:%M:%S'` mysqld_safe $*"
  echo "$msg"
  case $logging in
    init) ;;  # Just echo the message, don't save it anywhere
    file) echo "$msg" >> "$err_log" ;;
    syslog) logger -t "$syslog_tag_mysqld_safe" -p "$priority" "$*" ;;
    *)
      echo "Internal program error (non-fatal):" \
           " unknown logging method '$logging'" >&2
      ;;
  esac
}

log_error () {
  log_generic daemon.error "$@" >&2
}

log_notice () {
  log_generic daemon.notice "$@"
}

eval_log_error () {
  cmd="$1"
  case $logging in
    file) cmd="$cmd >> "`shell_quote_string "$err_log"`" 2>&1" ;;
    syslog)
      # mysqld often prefixes its messages with a timestamp, which is
      # redundant when logging to syslog (which adds its own timestamp)
      # However, we don't strip the timestamp with sed here, because
      # sed buffers output (only GNU sed supports a -u (unbuffered) option)
      # which means that messages may not get sent to syslog until the
      # mysqld process quits.
      cmd="$cmd 2>&1 | logger -t '$syslog_tag_mysqld' -p daemon.error"
      ;;
    *)
      echo "Internal program error (non-fatal):" \
           " unknown logging method '$logging'" >&2
      ;;
  esac

  #echo "Running mysqld: [$cmd]"
  eval "$cmd"
}

shell_quote_string() {
  # This sed command makes sure that any special chars are quoted,
  # so the arg gets passed exactly to the server.
  echo "$1" | sed -e 's,\([^a-zA-Z0-9/_.=-]\),\\\1,g'
}

parse_arguments() {
  # We only need to pass arguments through to the server if we don't
  # handle them here.  So, we collect unrecognized options (passed on
  # the command line) into the args variable.
  pick_args=
  if test "$1" = PICK-ARGS-FROM-ARGV
  then
    pick_args=1
    shift
  fi

  for arg do
    val=`echo "$arg" | sed -e "s;--[^=]*=;;"`
    case "$arg" in
      # these get passed explicitly to mysqld
      --basedir=*) MY_BASEDIR_VERSION="$val" ;;
      --datadir=*|--data=*) DATADIR="$val" ;;
      --pid-file=*) pid_file="$val" ;;
      --user=*) user="$val"; SET_USER=1 ;;
      --log-basename=*|--hostname=*|--loose-log-basename=*)
        pid_file="$val.pid";
	err_log="$val.err";
	;;

      # these might have been set in a [mysqld_safe] section of my.cnf
      # they are added to mysqld command line to override settings from my.cnf
      --log-error=*) err_log="$val" ;;
      --port=*) mysql_tcp_port="$val" ;;
      --socket=*) mysql_unix_port="$val" ;;

      # mysqld_safe-specific options - must be set in my.cnf ([mysqld_safe])!
      --core-file-size=*) core_file_size="$val" ;;
      --ledir=*) ledir="$val" ;;
      --mysqld=*) MYSQLD="$val" ;;
      --mysqld-version=*)
        if test -n "$val"
        then
          MYSQLD="mysqld-$val"
        else
          MYSQLD="mysqld"
        fi
        ;;
      --nice=*) niceness="$val" ;;
      --open-files-limit=*) open_files="$val" ;;
      --open_files_limit=*) open_files="$val" ;;
      --skip-kill-mysqld*) KILL_MYSQLD=0 ;;
      --syslog) want_syslog=1 ;;
      --skip-syslog) want_syslog=0 ;;
      --syslog-tag=*) syslog_tag="$val" ;;
      --timezone=*) TZ="$val"; export TZ; ;;

      --help) usage ;;

      *)
        if test -n "$pick_args"
        then
          append_arg_to_args "$arg"
        fi
        ;;
    esac
  done
}


#
# First, try to find BASEDIR and ledir (where mysqld is)
#

if echo '@pkgdatadir@' | grep '^@prefix@' > /dev/null
then
  relpkgdata=`echo '@pkgdatadir@' | sed -e 's,^@prefix@,,' -e 's,^/,,' -e 's,^,./,'`
else
  # pkgdatadir is not relative to prefix
  relpkgdata='@pkgdatadir@'
fi

MY_PWD=`pwd`
# Check for the directories we would expect from a binary release install
if test -n "$MY_BASEDIR_VERSION" -a -d "$MY_BASEDIR_VERSION"
then
  # BASEDIR is already overridden on command line.  Do not re-set.

  # Use BASEDIR to discover le.
  if test -x "$MY_BASEDIR_VERSION/libexec/mysqld"
  then
    ledir="$MY_BASEDIR_VERSION/libexec"
  else
    ledir="$MY_BASEDIR_VERSION/bin"
  fi
elif test -f "$relpkgdata"/english/errmsg.sys -a -x "$MY_PWD/bin/mysqld"
then
  MY_BASEDIR_VERSION="$MY_PWD"		# Where bin, share and data are
  ledir="$MY_PWD/bin"			# Where mysqld is
# Check for the directories we would expect from a source install
elif test -f "$relpkgdata"/english/errmsg.sys -a -x "$MY_PWD/libexec/mysqld"
then
  MY_BASEDIR_VERSION="$MY_PWD"		# Where libexec, share and var are
  ledir="$MY_PWD/libexec"		# Where mysqld is
# Since we didn't find anything, used the compiled-in defaults
else
  MY_BASEDIR_VERSION='@prefix@'
  ledir='@libexecdir@'
fi


#
# Second, try to find the data directory
#

# Try where the binary installs put it
if test -d $MY_BASEDIR_VERSION/data/mysql
then
  DATADIR=$MY_BASEDIR_VERSION/data
  if test -z "$defaults" -a -r "$DATADIR/my.cnf"
  then
    defaults="--defaults-extra-file=$DATADIR/my.cnf"
  fi
# Next try where the source installs put it
elif test -d $MY_BASEDIR_VERSION/var/mysql
then
  DATADIR=$MY_BASEDIR_VERSION/var
# Or just give up and use our compiled-in default
else
  DATADIR=@localstatedir@
fi

if test -z "$MYSQL_HOME"
then 
  if test -r "$MY_BASEDIR_VERSION/my.cnf" && test -r "$DATADIR/my.cnf"
  then
    log_error "WARNING: Found two instances of my.cnf -
$MY_BASEDIR_VERSION/my.cnf and
$DATADIR/my.cnf
IGNORING $DATADIR/my.cnf"

    MYSQL_HOME=$MY_BASEDIR_VERSION
  elif test -r "$DATADIR/my.cnf"
  then
    log_error "WARNING: Found $DATADIR/my.cnf
The data directory is a deprecated location for my.cnf, please move it to
$MY_BASEDIR_VERSION/my.cnf"
    MYSQL_HOME=$DATADIR
  else
    MYSQL_HOME=$MY_BASEDIR_VERSION
  fi
fi
export MYSQL_HOME


# Get first arguments from the my.cnf file, groups [mysqld] and [mysqld_safe]
# and then merge with the command line arguments
if test -x "$MY_BASEDIR_VERSION/bin/my_print_defaults"
then
  print_defaults="$MY_BASEDIR_VERSION/bin/my_print_defaults"
elif test -x ./bin/my_print_defaults
then
  print_defaults="./bin/my_print_defaults"
elif test -x @bindir@/my_print_defaults
then
  print_defaults="@bindir@/my_print_defaults"
elif test -x @bindir@/mysql_print_defaults
then
  print_defaults="@bindir@/mysql_print_defaults"
else
  print_defaults="my_print_defaults"
fi

append_arg_to_args () {
  args="$args "`shell_quote_string "$1"`
}

args=

SET_USER=2
parse_arguments `$print_defaults $defaults --loose-verbose mysqld mariadb server client-server`
if test $SET_USER -eq 2
then
  SET_USER=0
fi

parse_arguments `$print_defaults $defaults --loose-verbose mysqld_safe safe_mysqld`
parse_arguments PICK-ARGS-FROM-ARGV "$@"

# Determine what logging facility to use

# Ensure that 'logger' exists, if it's requested
if [ $want_syslog -eq 1 ]
then
  my_which logger > /dev/null 2>&1
  if [ $? -ne 0 ]
  then
    log_error "--syslog requested, but no 'logger' program found.  Please ensure that 'logger' is in your PATH, or do not specify the --syslog option to mysqld_safe."
    exit 1
  fi
fi

if [ -n "$err_log" -o $want_syslog -eq 0 ]
then
  if [ -n "$err_log" ]
  then
    # mysqld adds ".err" if there is no extension on the --log-error
    # argument; must match that here, or mysqld_safe will write to a
    # different log file than mysqld

    # mysqld does not add ".err" to "--log-error=foo."; it considers a
    # trailing "." as an extension
    if expr "$err_log" : '.*\.[^/]*$' > /dev/null
    then
        :
    else
      err_log="$err_log".err
    fi

    case "$err_log" in
      /* ) ;;
      * ) err_log="$DATADIR/$err_log" ;;
    esac
  else
    err_log=$DATADIR/`@HOSTNAME@`.err
  fi

  append_arg_to_args "--log-error=$err_log"

  if [ $want_syslog -eq 1 ]
  then
    # User explicitly asked for syslog, so warn that it isn't used
    log_error "Can't log to error log and syslog at the same time.  Remove all --log-error configuration options for --syslog to take effect."
  fi

  # Log to err_log file
  log_notice "Logging to '$err_log'."
  logging=file

  if [ ! -e "$err_log" ]; then                  # if error log already exists,
    touch "$err_log"                            # we just append. otherwise,
    chmod "$fmode" "$err_log"                   # fix the permissions here!
  fi

else
  if [ -n "$syslog_tag" ]
  then
    # Sanitize the syslog tag
    syslog_tag=`echo "$syslog_tag" | sed -e 's/[^a-zA-Z0-9_-]/_/g'`
    syslog_tag_mysqld_safe="${syslog_tag_mysqld_safe}-$syslog_tag"
    syslog_tag_mysqld="${syslog_tag_mysqld}-$syslog_tag"
  fi
  log_notice "Logging to syslog."
  logging=syslog
fi

USER_OPTION=""
if test -w / -o "$USER" = "root"
then
  if test "$user" != "root" -o $SET_USER = 1
  then
    USER_OPTION="--user=$user"
  fi
  # Change the err log to the right user, if it is in use
  if [ $want_syslog -eq 0 ]; then
    touch "$err_log"
    chown $user "$err_log"
  fi
  if test -n "$open_files"
  then
    ulimit -n $open_files
  fi
fi

if test -n "$open_files"
then
  append_arg_to_args "--open-files-limit=$open_files"
fi

safe_mysql_unix_port=${mysql_unix_port:-${MYSQL_UNIX_PORT:-@MYSQL_UNIX_ADDR@}}
# Make sure that directory for $safe_mysql_unix_port exists
mysql_unix_port_dir=`dirname $safe_mysql_unix_port`
if [ ! -d $mysql_unix_port_dir ]
then
  if ! `mkdir -p $mysql_unix_port_dir`
  then
    echo "Fatal error Can't create database directory '$mysql_unix_port'"
    exit 1
  fi
  chown $user $mysql_unix_port_dir
  chmod 755 $mysql_unix_port_dir
fi

# If the user doesn't specify a binary, we assume name "mysqld"
if test -z "$MYSQLD"
then
  MYSQLD=mysqld
fi

if test ! -x "$ledir/$MYSQLD"
then
  log_error "The file $ledir/$MYSQLD
does not exist or is not executable. Please cd to the mysql installation
directory and restart this script from there as follows:
./bin/mysqld_safe&
See http://dev.mysql.com/doc/mysql/en/mysqld-safe.html for more information"
  exit 1
fi

if test -z "$pid_file"
then
  pid_file="`@HOSTNAME@`.pid"
fi
# MariaDB wants pid file without datadir
append_arg_to_args "--pid-file=$pid_file"
case "$pid_file" in
  /* ) ;;
  * )  pid_file="$DATADIR/$pid_file" ;;
esac

if test -n "$mysql_unix_port"
then
  append_arg_to_args "--socket=$mysql_unix_port"
fi
if test -n "$mysql_tcp_port"
then
  append_arg_to_args "--port=$mysql_tcp_port"
fi

if test $niceness -eq 0
then
  NOHUP_NICENESS="nohup"
else
  NOHUP_NICENESS="nohup nice -$niceness"
fi

# Using nice with no args to get the niceness level is GNU-specific.
# This check could be extended for other operating systems (e.g.,
# BSD could use "nohup sh -c 'ps -o nice -p $$' | tail -1").
# But, it also seems that GNU nohup is the only one which messes
# with the priority, so this is okay.
if nohup nice > /dev/null 2>&1
then
    normal_niceness=`nice`
    nohup_niceness=`nohup nice 2>/dev/null`

    numeric_nice_values=1
    for val in $normal_niceness $nohup_niceness
    do
        case "$val" in
            -[0-9] | -[0-9][0-9] | -[0-9][0-9][0-9] | \
             [0-9] |  [0-9][0-9] |  [0-9][0-9][0-9] )
                ;;
            * )
                numeric_nice_values=0 ;;
        esac
    done

    if test $numeric_nice_values -eq 1
    then
        nice_value_diff=`expr $nohup_niceness - $normal_niceness`
        if test $? -eq 0 && test $nice_value_diff -gt 0 && \
            nice --$nice_value_diff echo testing > /dev/null 2>&1
        then
            # nohup increases the priority (bad), and we are permitted
            # to lower the priority with respect to the value the user
            # might have been given
            niceness=`expr $niceness - $nice_value_diff`
            NOHUP_NICENESS="nice -$niceness nohup"
        fi
    fi
else
    if nohup echo testing > /dev/null 2>&1
    then
        :
    else
        # nohup doesn't work on this system
        NOHUP_NICENESS=""
    fi
fi

# Try to set the core file size (even if we aren't root) because many systems
# don't specify a hard limit on core file size.
if test -n "$core_file_size"
then
  ulimit -c $core_file_size
fi

#
# If there exists an old pid file, check if the daemon is already running
# Note: The switches to 'ps' may depend on your operating system
if test -f "$pid_file"
then
  PID=`cat "$pid_file"`
  if @CHECK_PID@
  then
    if @FIND_PROC@
    then    # The pid contains a mysqld process
      log_error "A mysqld process already exists"
      exit 1
    fi
  fi
  rm -f "$pid_file"
  if test -f "$pid_file"
  then
    log_error "Fatal error: Can't remove the pid file:
$pid_file
Please remove it manually and start $0 again;
mysqld daemon not started"
    exit 1
  fi
fi

#
# Uncomment the following lines if you want all tables to be automatically
# checked and repaired during startup. You should add sensible key_buffer
# and sort_buffer values to my.cnf to improve check performance or require
# less disk space.
# Alternatively, you can start mysqld with the "myisam-recover" option. See
# the manual for details.
#
# echo "Checking tables in $DATADIR"
# $MY_BASEDIR_VERSION/bin/myisamchk --silent --force --fast --medium-check $DATADIR/*/*.MYI
# $MY_BASEDIR_VERSION/bin/isamchk --silent --force $DATADIR/*/*.ISM

# Does this work on all systems?
#if type ulimit | grep "shell builtin" > /dev/null
#then
#  ulimit -n 256 > /dev/null 2>&1		# Fix for BSD and FreeBSD systems
#fi

cmd="$NOHUP_NICENESS"

for i in  "$ledir/$MYSQLD" "$defaults" "--basedir=$MY_BASEDIR_VERSION" \
  "--datadir=$DATADIR" "$USER_OPTION"
do
  cmd="$cmd "`shell_quote_string "$i"`
done
cmd="$cmd $args"
# Avoid 'nohup: ignoring input' warning
test -n "$NOHUP_NICENESS" && cmd="$cmd < /dev/null"

log_notice "Starting $MYSQLD daemon with databases from $DATADIR"
while true
do
  rm -f $safe_mysql_unix_port "$pid_file"	# Some extra safety

  eval_log_error "$cmd"

  if [ $want_syslog -eq 0 -a ! -e "$err_log" ]; then
    touch "$err_log"                    # hypothetical: log was renamed but not
    chown $user "$err_log"              # flushed yet. we'd recreate it with
    chmod "$fmode" "$err_log"           # wrong owner next time we log, so set
  fi                                    # it up correctly while we can!

  if test ! -f "$pid_file"		# This is removed if normal shutdown
  then
    break
  fi

  if @TARGET_LINUX@ && test $KILL_MYSQLD -eq 1
  then
    # Test if one process was hanging.
    # This is only a fix for Linux (running as base 3 mysqld processes)
    # but should work for the rest of the servers.
    # The only thing is ps x => redhat 5 gives warnings when using ps -x.
    # kill -9 is used or the process won't react on the kill.
    numofproces=`ps xaww | grep -v "grep" | grep "$ledir/$MYSQLD\>" | grep -c "pid-file=$pid_file"`

    log_notice "Number of processes running now: $numofproces"
    I=1
    while test "$I" -le "$numofproces"
    do 
      PROC=`ps xaww | grep "$ledir/$MYSQLD\>" | grep -v "grep" | grep "pid-file=$pid_file" | sed -n '$p'` 

      for T in $PROC
      do
        break
      done
      #    echo "TEST $I - $T **"
      if kill -9 $T
      then
        log_error "$MYSQLD process hanging, pid $T - killed"
      else
        break
      fi
      I=`expr $I + 1`
    done
  fi
  log_notice "mysqld restarted"
done

log_notice "mysqld from pid file $pid_file ended"

