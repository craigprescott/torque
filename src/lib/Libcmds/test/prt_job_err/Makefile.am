PROG_ROOT = ../..

AM_CFLAGS = -g -DTEST_FUNCTION -I${PROG_ROOT}/ -I${PROG_ROOT}/${PBS_MACH} --coverage

lib_LTLIBRARIES = libprt_job_err.la

AM_LDFLAGS = @CHECK_LIBS@ ${lib_LTLIBRARIES}

check_PROGRAMS = test_prt_job_err

libprt_job_err_la_SOURCES = scaffolding.c ${PROG_ROOT}/prt_job_err.c
libprt_job_err_la_LDFLAGS = @CHECK_LIBS@ -shared -lgcov

test_prt_job_err_SOURCES = test_prt_job_err.c

check_SCRIPTS = ${PROG_ROOT}/../../test/coverage_run.sh

TESTS = ${check_PROGRAMS} ${check_SCRIPTS}

CLEANFILES = *.gcno *.gcda *.gcov core *.lo
