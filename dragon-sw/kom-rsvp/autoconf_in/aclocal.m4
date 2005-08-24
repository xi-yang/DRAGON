dnl
dnl Definition directory
dnl 01 -> return value checks
dnl       MN_STRERROR_R
dnl 02 -> standard functions parameter checks
dnl       MN_HAS_SO_REUSEPORT
dnl       MN_ACCEPT_P3
dnl       MN_GETSOCKOPT_P5
dnl       MN_GETSOCKNAME_P3
dnl       MN_SVC_REGISTER_P4
dnl       MN_RECVFROM_P2
dnl       MN_RECVFROM_P6
dnl       MN_SENDTO_P2
dnl 03 -> sizes of data types
dnl       MN_3BYTE_BITFIELD_SIZE
dnl 04 -> existance of compiler macros
dnl       MN_HAVE_MACRO_PRETTY_FUNCTION
dnl 05 -> existance of data types
dnl       MN_HAVE_TYPE_BOOL
dnl 06 -> existance of keywords
dnl       MN_HAVE_KEYWORD_FALSE
dnl       MN_HAVE_KEYWORD_TRUE
dnl 07 -> configure command line switches
dnl       MN_ARG_ADDPATH
dnl 08 -> test for an dynamic library search path
dnl       MN_TRY_RPATH
dnl 09 -> test for programs
dnl       MN_MAKEDEP
dnl       MN_MAKEDEPXX
dnl

dnl
dnl ----------------------------------------
dnl return value checks
dnl ----------------------------------------
dnl
AC_DEFUN(MN_STRERROR_R,
[
  AC_CACHE_CHECK(return value of strerror_r,
    mn_cv_have_func_strerror_r,
    [ AC_EGREP_CPP(
        "int.*strerror_r",
        [ #include <pthread.h>
          #include <string.h>
        ],
        [ mn_cv_have_func_strerror_r="int"
        ],
        AC_EGREP_CPP(
          "char.*strerror_r",
          [ #include <pthread.h>
            #include <string.h>
          ],
          [ mn_cv_have_func_strerror_r="char*"
          ],
          [ mn_cv_have_func_strerror_r="unknown"
	    AC_MSG_ERROR(can not continue known return type)
          ])
      )
    ])
  if test "$mn_cv_have_func_strerror_r" = "int"; then
    AC_DEFINE(STRERROR_R_TYPE_INT)
  else
    if test "$mn_cv_have_func_strerror_r" = "char*"; then
      AC_DEFINE(STRERROR_R_TYPE_CHARP)
    fi
  fi
])

dnl
dnl ----------------------------------------
dnl sin len check 
dnl ----------------------------------------
dnl
AC_DEFUN(MN_HAVE_SIN_LEN,
[
  AC_CACHE_CHECK(
    [whether struct sockaddr_in has sin_len],
    mn_cv_func_sin_len,
    [ AC_TRY_COMPILE(
      [#include <sys/types.h>
       #include <sys/socket.h>
       #include <netinet/in.h>
      ],
      [ struct sockaddr_in x;
	x.sin_len = sizeof (x);
      ],
      [ mn_cv_func_sin_len="yes"
      ],
      [ mn_cv_func_sin_len="no"
      ])
    ])
  if test "Xyes" = "X$mn_cv_func_sin_len" ; then
    AC_DEFINE(HAVE_SIN_LEN)
  fi
])

dnl
dnl ----------------------------------------
dnl standard functions parameter checks
dnl ----------------------------------------
dnl
AC_DEFUN(MN_ACCEPT_P3,
[
  AC_CACHE_CHECK(
    [for 2nd parameter of accept],
    mn_cv_func_accept_p2,
    [ AC_TRY_COMPILE(
      [#include <sys/types.h>
       #include <sys/socket.h>
      ],
      [ socklen_t t;
        accept(0,0,&t);
      ],
      [ mn_cv_func_accept_p2="socklen_t"
      ],
      [ AC_TRY_COMPILE(
	[#include <sys/types.h>
	#include <sys/socket.h>
	],
        [ size_t t;
          accept(0,0,&t);
        ],
	[ mn_cv_func_accept_p2="size_t"
	],
	[ mn_cv_func_accept_p2="int"
	])
      ])
    ])
  AC_DEFINE_UNQUOTED(ACCEPT_SIZE_T,$mn_cv_func_accept_p2)
])

AC_DEFUN(MN_HAS_SO_REUSEPORT,
[
  AC_CACHE_CHECK(
    [whether SO_REUSEPORT is defined],
    mn_cv_func_reuseport,
    [ AC_TRY_COMPILE(
      [#include <sys/types.h>
       #include <sys/socket.h>
      ],
      [ const int on = 1;
        setsockopt( 0, SOL_SOCKET, SO_REUSEPORT, (char*)&on, sizeof(on) );
      ],
      [ mn_cv_func_reuseport="SO_REUSEPORT"
      ],
      [ mn_cv_func_reuseport="SO_REUSEADDR"
      ])
    ])
  AC_DEFINE_UNQUOTED(SO_REUSEXXX,$mn_cv_func_reuseport)
])

AC_DEFUN(MN_GETSOCKOPT_P5,
[
  AC_CACHE_CHECK(
    [for 5th parameter of getsockopt],
    mn_cv_func_getsockopt_p5,
    [ AC_TRY_COMPILE(
      [#include <sys/types.h>
       #include <sys/socket.h>
      ],
      [ socklen_t t;
        getsockopt(0,0,0,0,&t);
      ],
      [ mn_cv_func_getsockopt_p5="socklen_t"
      ],
      [ AC_TRY_COMPILE(
	[#include <sys/types.h>
	#include <sys/socket.h>
	],
        [ size_t t;
          getsockopt(0,0,0,0,&t);
        ],
	[ mn_cv_func_getsockopt_p5="size_t"
	],
	[ mn_cv_func_getsockopt_p5="int"
	])
      ])
    ])
  AC_DEFINE_UNQUOTED(GETSOCKOPT_SIZE_T,$mn_cv_func_getsockopt_p5)
])

AC_DEFUN(MN_GETSOCKNAME_P3,
[
  AC_CACHE_CHECK(
    [for 3rd parameter of getsockname],
    mn_cv_func_getsockname_p3,
    [ AC_TRY_COMPILE(
      [#include <sys/types.h>
       #include <sys/socket.h>
      ],
      [ socklen_t t;
        getsockname(0,0,&t);
      ],
      [ mn_cv_func_getsockname_p3="socklen_t"
      ],
      [ AC_TRY_COMPILE(
	[#include <sys/types.h>
	#include <sys/socket.h>
	],
        [ size_t t;
          getsockname(0,0,&t);
        ],
	[ mn_cv_func_getsockname_p3="size_t"
	],
	[ mn_cv_func_getsockname_p3="int"
	])
      ])
    ])
  AC_DEFINE_UNQUOTED(GETSOCKNAME_SIZE_T,$mn_cv_func_getsockname_p3)
])

AC_DEFUN(MN_SVC_REGISTER_P4,
[
  AC_CACHE_CHECK(
    [for 4th parameter of svc_register],
    mn_cv_func_svc_register_p4,
    [ AC_TRY_COMPILE(
      [#include <rpc/rpc.h>
       #include <rpc/svc.h>
      ],
      [ typedef void(*fun)();
        fun x = 0;
        svc_register(0,0,0,x,0);
      ],
      [ mn_cv_func_svc_register_p4="void(*)()"
      ],
      [ AC_TRY_COMPILE(
        [#include <rpc/rpc.h>
         #include <rpc/svc.h>
        ],
        [ typedef void(*fun)(struct svc_req*, SVCXPRT*);
          fun x = 0;
          svc_register(0,0,0,x,0);
        ],
        [ mn_cv_func_svc_register_p4="void(*)(svc_req*,SVCXPRT*)"
        ],
        [ AC_TRY_COMPILE(
          [#include <rpc/rpc.h>
           #include <rpc/svc.h>
           #include <rpc/svc_soc.h>
          ],
          [ typedef void(*fun)(struct svc_req*, SVCXPRT*);
            fun x = 0;
            svc_register(0,0,0,x,0);
          ],
          [ mn_cv_func_svc_register_p4="void(*)(svc_req*,SVCXPRT*)"
          ],
          [ mn_cv_func_svc_register_p4="not identified"
          ])
        ])
      ])
    ])
  if test "$mn_cv_func_svc_register_p4" = "void(*)()"; then
    AC_DEFINE(SVC_REGISTER_PROTO_ARG,1)
  else
    if test "$mn_cv_func_svc_register_p4" = "void(*)(svc_req*,SVCXPRT*)"; then
      AC_DEFINE(SVC_REGISTER_PROTO_ARG,2)
    else
      AC_MSG_ERROR(can not continue)
    fi
  fi
])

AC_DEFUN(MN_RECVFROM_P2,
[
  AC_CACHE_CHECK(
    [for 2nd parameter of recvfrom],
    mn_cv_func_recvfrom_p2,
    [ AC_TRY_COMPILE(
      [#include <sys/types.h>
       #include <sys/socket.h>
      ],
      [ char buf[10];
        recvfrom(0,(void*)buf,0,0,0,0);
      ],
      [ mn_cv_func_recvfrom_p2="void*"
      ],
      [ mn_cv_func_recvfrom_p2="char*"
      ])
    ])
  AC_DEFINE_UNQUOTED(RECVFROM_BUF_T,$mn_cv_func_recvfrom_p2)
])

AC_DEFUN(MN_RECVFROM_P6,
[
  AC_CACHE_CHECK(
    [for 6th parameter of recvfrom],
    mn_cv_func_recvfrom_p6,
    [ AC_TRY_COMPILE(
      [#include <sys/types.h>
       #include <sys/socket.h>
      ],
      [ socklen_t t;
        recvfrom(0,0,0,0,0,&t);
      ],
      [ mn_cv_func_recvfrom_p6=socklen_t
      ],
      [ AC_TRY_COMPILE(
        [#include <sys/types.h>
         #include <sys/socket.h>
        ],
        [ size_t t;
          recvfrom(0,0,0,0,0,&t);
        ],
        [ mn_cv_func_recvfrom_p6=size_t
        ],
        [ mn_cv_func_recvfrom_p6=int
        ])
      ])
    ])
  AC_DEFINE_UNQUOTED(RECVFROM_SIZE_T,$mn_cv_func_recvfrom_p6)
])

AC_DEFUN(MN_SENDTO_P2,
[
  AC_CACHE_CHECK(
    [for 2nd parameter of sendto],
    mn_cv_func_sendto_p2,
    [ AC_TRY_COMPILE(
      [#include <sys/types.h>
       #include <sys/socket.h>
      ],
      [ char buf[10];
        sendto(0,(const void*)buf,0,0,0,0);
      ],
      [ mn_cv_func_sendto_p2="const void*"
      ],
      [ AC_TRY_COMPILE(
        [#include <sys/types.h>
         #include <sys/socket.h>
        ],
        [ char buf[10];
          sendto(0,(void*)buf,0,0,0,0);
        ],
        [ mn_cv_func_sendto_p2="void*"
        ],
        [ mn_cv_func_sendto_p2="char*"
        ])
      ])
    ])
  AC_DEFINE_UNQUOTED(SENDTO_BUF_T,$mn_cv_func_sendto_p2)
])

dnl
dnl ----------------------------------------
dnl sizes of data types
dnl ----------------------------------------
dnl
AC_DEFUN(MN_3BYTE_BITFIELD_SIZE,
[
  AC_CACHE_CHECK(
    [whether int y:24 takes 3 bytes],
    mn_cv_size_3byte_bitfield,
    AC_TRY_RUN(
      [  #include <stdio.h>
         typedef struct {
           unsigned int x:8;
           int y:24;
         } testtype;
         int main() {
           if (sizeof(testtype)==4) exit(0); else exit(1000);
         }
      ],
      mn_cv_size_3byte_bitfield=yes,
      mn_cv_size_3byte_bitfield=no))
  if test "$mn_cv_size_3byte_bitfield" = "no"; then
    AC_MSG_ERROR(unsupported case, you must modify the source code!)
  fi
])

dnl
dnl ----------------------------------------
dnl existance of compiler macros
dnl ----------------------------------------
dnl
AC_DEFUN(MN_HAVE_MACRO_PRETTY_FUNCTION,
[
  AC_CACHE_CHECK(
    [whether macro __PRETTY_FUNCTION__ is resolved],
    mv_cv_have_macro_pretty_function,
    AC_TRY_COMPILE(
      [ ],
      [ const char* c = __PRETTY_FUNCTION__;
      ],
      [ mv_cv_have_macro_pretty_function=yes
      ],
      [ mv_cv_have_macro_pretty_function=no
      ])
    )
  if test "$mv_cv_have_macro_pretty_function" = "no" ; then
    AC_DEFINE(__PRETTY_FUNCTION__,__FUNCTION__)
  fi
])

dnl
dnl ----------------------------------------
dnl existance of data typesdnl ----------------------------------------
dnl
AC_DEFUN(MN_HAVE_TYPE_BOOL,
[
  AC_CACHE_CHECK(
    [whether bool is a data type],
    ac_cv_have_type_bool,
    AC_TRY_COMPILE(
      [#include <sys/types.h>
       #include <stdlib.h>
      ],
      [ bool t;
      ],
      [ ac_cv_have_type_bool=yes
      ],
      [ ac_cv_have_type_bool=no
      ])
    )
  if test "$ac_cv_have_type_bool" = "yes"; then
    AC_DEFINE(HAVE_KEYWORD_BOOL)
  fi
])

dnl
dnl ----------------------------------------
dnl existance of keywords
dnl ----------------------------------------
dnl
AC_DEFUN(MN_HAVE_KEYWORD_FALSE,
[
  AC_CACHE_CHECK(
    [whether false is a keyword],
    ac_cv_have_keyword_false,
    AC_TRY_COMPILE(
      [#include <sys/types.h>
       #include <stdlib.h>
      ],
      [ int t = false;
      ],
      [ ac_cv_have_keyword_false=yes
      ],
      [ ac_cv_have_keyword_false=no
      ])
    )
  if test "$ac_cv_have_keyword_false" = "yes"; then
    AC_DEFINE(HAVE_KEYWORD_FALSE)
  fi
])

AC_DEFUN(MN_HAVE_KEYWORD_TRUE,
[
  AC_CACHE_CHECK(
    [whether true is a keyword],
    ac_cv_have_keyword_true,
    AC_TRY_COMPILE(
      [#include <sys/types.h>
       #include <stdlib.h>
      ],
      [ int t = true;
      ],
      [ ac_cv_have_keyword_true=yes
      ],
      [ ac_cv_have_keyword_true=no
      ])
    )
  if test "$ac_cv_have_keyword_true" = "yes"; then
    AC_DEFINE(HAVE_KEYWORD_TRUE)
  fi
])

dnl
dnl ----------------------------------------
dnl configure command line switches
dnl ----------------------------------------
dnl
AC_DEFUN(MN_ARG_ADDPATH,
[ AC_ARG_WITH($1, $2,
    [ echo "Adding $with_$1/include and $with_$1/lib"
      CPPFLAGS="-I$with_$1/include $CPPFLAGS"
      INCLUDES="-I$with_$1/include $INCLUDES"
      LIBS="-L$with_$1/lib $LIBS"
    ])
])

dnl
dnl ----------------------------------------
dnl test for an dynamic library search path
dnl ----------------------------------------
dnl
AC_DEFUN(MN_TRY_RPATH_SUB,
[
  mn_save_ld_flags="$LIBS"
  LIBS="$LIBS $1 /tmp"
  AC_TRY_LINK([],
    [],
    [ LIBS="$mn_save_ld_flags"
      ac_cv_dynlib_searchpath_flag="$1" ],
    [ LIBS="$mn_save_ld_flags"
      $2 ])
])
AC_DEFUN(MN_TRY_RPATH,
[
  AC_CACHE_CHECK(
    [for dynamic search patch linker flag],
    ac_cv_dynlib_searchpath_flag,
    [ MN_TRY_RPATH_SUB([-rpath],
      [ MN_TRY_RPATH_SUB([-Wl,-rpath],
        [ MN_TRY_RPATH_SUB([-R],
          [ MN_TRY_RPATH_SUB([-Wl,-R],
	    ac_cv_dynlib_searchpath_flag="none"
	    )
	  ])
	])
      ])
    ])
  $1="$ac_cv_dynlib_searchpath_flag"
])

dnl
dnl ----------------------------------------
dnl test for programs
dnl ----------------------------------------
dnl
AC_DEFUN(MN_MAKEDEP,
[ AC_CACHE_CHECK(
    [for C makedepend progam],
    mn_cv_prog_makedep,
    [ if test $ac_cv_prog_gcc = yes; then
        mn_cv_prog_makedep="$CC -MM"
      else
	if test $ac_cv_path_MAKEDEPEND = no; then
          mn_cv_prog_makedep="echo"
	else
          mn_cv_prog_makedep=$ac_cv_path_MAKEDEPEND
	fi
      fi
    ])
  $1=$mn_cv_prog_makedep
])

AC_DEFUN(MN_MAKEDEPXX,
[ AC_CACHE_CHECK(
    [for C++ makedepend progam],
    mn_cv_prog_makedepxx,
    [ if test $ac_cv_prog_gcc = yes; then
        mn_cv_prog_makedepxx="$CXX -MM"
      else
	if test $ac_cv_path_MAKEDEPEND = no; then
          mn_cv_prog_makedepxx="echo"
	else
          mn_cv_prog_makedepxx="$ac_cv_path_MAKEDEPEND -I/usr/lpp/xlC/include"
	fi
      fi
    ])
  $1=$mn_cv_prog_makedepxx
])

