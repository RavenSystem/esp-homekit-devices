# ===========================================================================
#      https://github.com/BrianAker/ddm4/
# ===========================================================================
#
# SYNOPSIS
#
#   AX_COMPILER_VERSION()
#
# DESCRIPTION
#
#   Capture version of C/C++ compiler
#
# LICENSE
#
#  Copyright (C) 2012 Brian Aker
#  All rights reserved.
#  
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are
#  met:
#  
#      * Redistributions of source code must retain the above copyright
#  notice, this list of conditions and the following disclaimer.
#  
#      * Redistributions in binary form must reproduce the above
#  copyright notice, this list of conditions and the following disclaimer
#  in the documentation and/or other materials provided with the
#  distribution.
#  
#      * The names of its contributors may not be used to endorse or
#  promote products derived from this software without specific prior
#  written permission.
#  
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#serial 5
AC_DEFUN([_C_COMPILER_VERSION],
    [AC_MSG_CHECKING([C Compiler version])

    AS_CASE(["$ax_cv_c_compiler_vendor"],
      [sun],[ax_c_compiler_version=`$CC -V 2>&1 | sed 1q`],
      [intel],[ax_c_compiler_version=`$CC --version 2>&1 | sed 1q`],
      [clang],[ax_c_compiler_version=`$CC --version 2>&1 | sed 1q`],
      [gnu],[ax_c_compiler_version=`$CC --version | sed 1q`],
      [mingw],[ax_c_compiler_version=`$CC --version | sed 1q`],
      [ax_c_compiler_version="unknown: $ax_cv_c_compiler_vendor"])

    AC_MSG_RESULT(["$ax_c_compiler_version"])
    AC_SUBST([CC_VERSION_VENDOR],["$ax_cv_c_compiler_vendor"])
    AC_SUBST([CC_VERSION],["$ax_c_compiler_version"])
  ])

AC_DEFUN([_CXX_COMPILER_VERSION],
    [AC_MSG_CHECKING([C++ Compiler version])

    AS_CASE(["$ax_cv_c_compiler_vendor"],
      [sun],[ax_cxx_compiler_version=`$CXX -V 2>&1 | sed 1q`],
      [intel],[ax_cxx_compiler_version=`$CXX --version 2>&1 | sed 1q`],
      [clang],[ax_cxx_compiler_version=`$CXX --version 2>&1 | sed 1q`],
      [gnu],[ax_cxx_compiler_version=`$CXX --version | sed 1q`],
      [mingw],[ax_cxx_compiler_version=`$CXX --version | sed 1q`],
      [ax_cxx_compiler_version="unknown: $ax_cv_c_compiler_vendor"])

    AC_MSG_RESULT(["$ax_cxx_compiler_version"])
    AC_SUBST([CXX_VERSION_VENDOR],["$ax_cv_c_compiler_vendor"])
    AC_SUBST([CXX_VERSION],["$ax_cxx_compiler_version"])
    ])

AC_DEFUN([AX_COMPILER_VERSION],
    [AC_REQUIRE([AX_COMPILER_VENDOR])

    AC_MSG_CHECKING([MINGW])
    AC_CHECK_DECL([__MINGW32__],
      [MINGW=yes
      ax_c_compiler_version_vendor=mingw],
      [MINGW=no])
    AC_MSG_RESULT([$MINGW])

    AC_REQUIRE([_C_COMPILER_VERSION])
    AC_REQUIRE([_CXX_COMPILER_VERSION])
    AS_IF([test "x$GCC" = xyes],
      [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#if !defined(__GNUC__) || (__GNUC__ < 4) || ((__GNUC__ >= 4) && (__GNUC_MINOR__ < 7))
# error GCC is Too Old!
#endif
          ]])],
        [ac_c_gcc_recent=yes],
        [ac_c_gcc_recent=no])
      ])
    ])
