PROG_ROOT = ../..

AM_CFLAGS = -g -DTEST_FUNCTION -I${PROG_ROOT}/ -I${PROG_ROOT}/${PBS_MACH} --coverage

lib_LTLIBRARIES = libPBSD_sig2.la

AM_LDFLAGS = @CHECK_LIBS@ ${lib_LTLIBRARIES}

check_PROGRAMS = test_PBSD_sig2

libPBSD_sig2_la_SOURCES = scaffolding.c ${PROG_ROOT}/PBSD_sig2.c
libPBSD_sig2_la_LDFLAGS = @CHECK_LIBS@ -shared -lgcov

test_PBSD_sig2_SOURCES = test_PBSD_sig2.c

check_SCRIPTS = ${PROG_ROOT}/../../test/coverage_run.sh

TESTS = ${check_PROGRAMS} ${check_SCRIPTS} 

CLEANFILES = *.gcno *.gcda *.gcov core *.lo
