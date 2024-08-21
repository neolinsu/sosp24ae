AC_DEFUN([AX_VESSEL],[
tryvesseldir=""
AC_ARG_WITH(vessel,
       [  --with-vessel=PATH     Specify path to vessel installation ],
       [
                if test "x$withval" != "xno" ; then
                        tryvesseldir=$withval
                fi
       ]
)

dnl ------------------------------------------------------

AC_CACHE_CHECK([for vessel directory], ac_cv_vessel_dir, [
  saved_LIBS="$LIBS"
  saved_LDFLAGS="$LDFLAGS"
  saved_CPPFLAGS="$CPPFLAGS"
  le_found=no
  for ledir in $tryvesseldir ; do
    RUNTIME_LIBS=$(make -f $ledir/share/vessel/Makefile print-RUNTIME_LIBS | grep RUNTIME_LIBS | sed 's/.*= //g')
    RUNTIME_CONFIG=$(make -f $ledir/share/vessel/Makefile print-RUNTIME_CONFIG | grep RUNTIME_CONFIG | sed 's/.*= //g')

    LDFLAGS="$saved_LDFLAGS"
    LIBS="$RUNTIME_LIBS $saved_LIBS"

    # Skip the directory if it isn't there.
    if test ! -z "$ledir" -a ! -d "$ledir" ; then
       continue;
    fi
    if test ! -z "$ledir" ; then
        LDFLAGS="-L$ledir/lib64 -T $ledir/include/vessel/runtime/tls/tls.ld $LDFLAGS $LIBS"
        CPPFLAGS="$RUNTIME_CONFIG -I$ledir/include/vessel -I$ledir/include $CPPFLAGS"
    fi
    # Can I compile and link it?
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <sys/time.h>
#include <sys/types.h>
#include <runtime/thread.h>]], [[ thread_yield() ]])],[ vessel_linked=yes ],[ vessel_linked=no ])
    if test $vessel_linked = yes; then
       if test ! -z "$ledir" ; then
         ac_cv_vessel_dir=$ledir
       else
         ac_cv_vessel_dir="(system)"
       fi
       le_found=yes
       break
    else
    AC_MSG_ERROR([vessel is required!
      LDFLAGS = $LDFLAGS
      CPPFLAGS = $CPPFLAGS
])
    fi
  done
  LIBS="$saved_LIBS"
  LDFLAGS="$saved_LDFLAGS"
  CPPFLAGS="$saved_CPPFLAGS"
  if test $le_found = no ; then
    AC_MSG_ERROR([vessel is required. 

      If it's already installed, specify its path using --with-vessel=/dir/
])
  fi
])
VE_LIBS="$RUNTIME_LIBS -ldl"
if test $ac_cv_vessel_dir != "(system)"; then
    VE_LDFLAGS="-pie -L$ac_cv_vessel_dir/lib64 -T$ac_cv_vessel_dir/include/vessel/runtime/tls/tls.ld"
    le_libdir="$ac_cv_vessel_dir/lib64"
    VE_CPPFLAGS="-I$ac_cv_vessel_dir/include/vessel $RUNTIME_CONFIG"
fi
VE_CPPFLAGS="-fPIC -DNDEBUG -O3 -Wall -std=gnu11 -D_GNU_SOURCE -mssse3 -muintr -mrdpid -mfsgsbase $VE_CPPFLAGS"

])dnl AX_VESSEL

AC_ARG_ENABLE(cxltp,
  [AS_HELP_STRING([--enable-cxltp],[Enable cxltp])])

AC_DEFUN([AX_VESSEL_CXLTP],[
if test "x$enable_cxltp" == "xyes"; then
  AC_DEFINE([VESSEL_CXLTP],1,[Set to nonzero if you want to use cxltp])
fi
])