/*
*         OpenPBS (Portable Batch System) v2.3 Software License
*
* Copyright (c) 1999-2000 Veridian Information Solutions, Inc.
* All rights reserved.
*
* ---------------------------------------------------------------------------
* For a license to use or redistribute the OpenPBS software under conditions
* other than those described below, or to purchase support for this software,
* please contact Veridian Systems, PBS Products Department ("Licensor") at:
*
*    www.OpenPBS.org  +1 650 967-4675                  sales@OpenPBS.org
*                        877 902-4PBS (US toll-free)
* ---------------------------------------------------------------------------
*
* This license covers use of the OpenPBS v2.3 software (the "Software") at
* your site or location, and, for certain users, redistribution of the
* Software to other sites and locations.  Use and redistribution of
* OpenPBS v2.3 in source and binary forms, with or without modification,
* are permitted provided that all of the following conditions are met.
* After December 31, 2001, only conditions 3-6 must be met:
*
* 1. Commercial and/or non-commercial use of the Software is permitted
*    provided a current software registration is on file at www.OpenPBS.org.
*    If use of this software contributes to a publication, product, or
*    service, proper attribution must be given; see www.OpenPBS.org/credit.html
*
* 2. Redistribution in any form is only permitted for non-commercial,
*    non-profit purposes.  There can be no charge for the Software or any
*    software incorporating the Software.  Further, there can be no
*    expectation of revenue generated as a consequence of redistributing
*    the Software.
*
* 3. Any Redistribution of source code must retain the above copyright notice
*    and the acknowledgment contained in paragraph 6, this list of conditions
*    and the disclaimer contained in paragraph 7.
*
* 4. Any Redistribution in binary form must reproduce the above copyright
*    notice and the acknowledgment contained in paragraph 6, this list of
*    conditions and the disclaimer contained in paragraph 7 in the
*    documentation and/or other materials provided with the distribution.
*
* 5. Redistributions in any form must be accompanied by information on how to
*    obtain complete source code for the OpenPBS software and any
*    modifications and/or additions to the OpenPBS software.  The source code
*    must either be included in the distribution or be available for no more
*    than the cost of distribution plus a nominal fee, and all modifications
*    and additions to the Software must be freely redistributable by any party
*    (including Licensor) without restriction.
*
* 6. All advertising materials mentioning features or use of the Software must
*    display the following acknowledgment:
*
*     "This product includes software developed by NASA Ames Research Center,
*     Lawrence Livermore National Laboratory, and Veridian Information
*     Solutions, Inc.
*     Visit www.OpenPBS.org for OpenPBS software support,
*     products, and information."
*
* 7. DISCLAIMER OF WARRANTY
*
* THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT
* ARE EXPRESSLY DISCLAIMED.
*
* IN NO EVENT SHALL VERIDIAN CORPORATION, ITS AFFILIATED COMPANIES, OR THE
* U.S. GOVERNMENT OR ANY OF ITS AGENCIES BE LIABLE FOR ANY DIRECT OR INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
* This license will be governed by the laws of the Commonwealth of Virginia,
* without reference to its choice of law rules.
*/

#include <pbs_config.h>   /* the master config generated by configure */

#include <stdio.h>
#include "portability.h"
#include <errno.h>
#include <pthread.h>
#include <arpa/inet.h>
#include "log.h"
#include "../lib/Liblog/pbs_log.h"
#include "../lib/Liblog/log_event.h"
#include "pbs_ifl.h"
#include "net_connect.h"
#include "svrfunc.h"
#include "sched_cmds.h"
#include "pbs_error.h"
#include "dis_internal.h" /* DIS_tcp_setup */
#include "../lib/Libifl/lib_ifl.h" /* get_port_from_server_name_file */
#include "pbsd_main.h" /* process_pbs_server_port */
#include "process_request.h" /*process_request */

/* Global Data */

extern pbs_net_t pbs_scheduler_addr;
extern unsigned int pbs_scheduler_port;
extern char      server_name[];

extern struct connection svr_conn[];

extern pthread_mutex_t *svr_do_schedule_mutex;
extern int       svr_do_schedule;
extern char     *msg_sched_called;
extern char     *msg_sched_nocall;
extern char     *msg_listnr_called;
extern char     *msg_listnr_nocall;

extern struct listener_connection listener_conns[];

int scheduler_sock;
int scheduler_jobct;
int listener_command = SCH_SCHEDULE_NULL;
extern pthread_mutex_t *listener_command_mutex;
extern pthread_mutex_t *scheduler_sock_jobct_mutex;


/* Functions private to this file */

static int  put_4byte(int sock, unsigned int val);
static void listener_close(int);


extern ssize_t write_nonblocking_socket(int, const void *, ssize_t);

extern void scheduler_close();


/* sync w/sched_cmds.h */

const char *PSchedCmdType[] =
  {
  "NULL",
  "new",
  "term",
  "time",
  "recyc",
  "cmd",
  "N/A",
  "configure",
  "quit",
  "ruleset",
  "scheduler_first",
  NULL
  };





/*
 * contact_sched - open connection to the scheduler and send it a command
 */

void *contact_sched(

  void *new_cmd)  /* I */

  {
  int   sock;

  char  tmpLine[1024];
  char  EMsg[1024];

  char  log_buf[LOCAL_LOG_BUF_SIZE];
  int cmd = *(int *)new_cmd;

  free(new_cmd);

  if (LOGLEVEL >= 10)
    {
    sprintf(log_buf, "command %d", cmd);
    log_event(PBSEVENT_DEBUG, PBS_EVENTCLASS_SERVER, __func__, log_buf);
    }

  /* connect to the Scheduler */
  sock = client_to_svr(pbs_scheduler_addr, pbs_scheduler_port, 1, EMsg);

  if (sock < 0)
    {
    /* Thread exit */

    return(NULL);
    }

  add_scheduler_conn(
    sock,
    FromClientDIS,
    pbs_scheduler_addr,
    pbs_scheduler_port,
    PBS_SOCK_INET,
    NULL);

  pthread_mutex_lock(scheduler_sock_jobct_mutex);
  scheduler_sock = sock;
  pthread_mutex_unlock(scheduler_sock_jobct_mutex);

  pthread_mutex_lock(svr_conn[sock].cn_mutex);
  svr_conn[sock].cn_authen = PBS_NET_CONN_FROM_PRIVIL;
  pthread_mutex_unlock(svr_conn[sock].cn_mutex);

  /* send command to Scheduler */

  if (put_4byte(sock, cmd) < 0)
    {
    sprintf(tmpLine, "%s - port %d",
            msg_sched_nocall,
            pbs_scheduler_port);

    log_ext(errno, __func__, tmpLine, LOG_ALERT);

    close_conn(sock, FALSE);

    /* Thread exit */
    return(NULL);
    }

  close_conn(sock, FALSE);
  scheduler_close();

  /* Thread exit */
  return(NULL);
  }  /* END contact_sched() */





/*
 * schedule_jobs - contact scheduler and direct it to run a scheduling cycle
 * If a request is already outstanding, skip this one.
 *
 * Returns: -1 = error
 *    0 = scheduler notified
 *   +1 = scheduler busy
 */

int schedule_jobs(void)

  {
  int cmd;

  static int first_time = 1;
  int tmp_sched_sock = -1;
  pthread_t tid;
  pthread_attr_t t_attr;
  int   *new_cmd = NULL;

  pthread_mutex_lock(svr_do_schedule_mutex);

  if (first_time)
    cmd = SCH_SCHEDULE_FIRST;
  else
    cmd = svr_do_schedule;

  /*listener_command = cmd;*/

  svr_do_schedule = SCH_SCHEDULE_NULL;
  pthread_mutex_unlock(svr_do_schedule_mutex);

  pthread_mutex_lock(scheduler_sock_jobct_mutex);
  if (scheduler_sock == -1)
    scheduler_jobct = 0;
  else
    tmp_sched_sock = scheduler_sock;
  pthread_mutex_unlock(scheduler_sock_jobct_mutex);

  if (tmp_sched_sock == -1)
    {
    if (pthread_attr_init(&t_attr) != 0)
      {
      /* Can not init thread attribute structure */
      perror("could not create listener thread for scheduler");
      log_err(-1, __func__, "Failed to create listener thread for scheduler");
      }
    else if (pthread_attr_setdetachstate(&t_attr, PTHREAD_CREATE_DETACHED) != 0)
      {
      /* Can not set thread initial state as detached */
      pthread_attr_destroy(&t_attr);
      perror("could not detach listener thread for scheduler");
      log_err(-1, __func__, "Failed to detach listener thread for scheduler");
      }
    else
      {
      new_cmd = (int *)calloc(1, sizeof(int));
      if (!new_cmd)
        {
        log_err(ENOMEM,__func__,"Could not allocate memory to set command");
        return(-1);
        }
      *new_cmd = cmd;

      if (pthread_create(&tid, &t_attr, contact_sched, (void *)new_cmd)
   != 0)
        {
        perror("could not start listener thread for scheduler");
        log_err(-1, __func__, "Failed to start listener thread for scheduler");
      return(-1);
        }
      }

    pthread_attr_destroy(&t_attr);

    first_time = 0;

    return(0);
    }
  return(1);
  }  /* END schedule_jobs() */



void *start_process_request(void *vp)
  {
  int sock = *(int *)vp;
  struct tcp_chan *chan = NULL;
  if ((chan = DIS_tcp_setup(sock)) == NULL)
    return NULL;
  process_request(chan);
  DIS_tcp_cleanup(chan);
  return(NULL);
  }

/*
 * contact_listener - open connection to the listener and send it a command
 */

static int contact_listener(

  int l_idx)  /* I */

  {
  int   sock;

  char  tmpLine[1024];
  char  EMsg[1024];

  char  log_buf[LOCAL_LOG_BUF_SIZE];

  /* If this is the first time contacting the scheduler for
   * this listener set the cmd */
  if (listener_conns[l_idx].first_time)
    {
    pthread_mutex_lock(listener_command_mutex);
    listener_command = SCH_SCHEDULE_FIRST;
    pthread_mutex_unlock(listener_command_mutex);
    }

  /* connect to the Listener */
  sock = client_to_svr(listener_conns[l_idx].address,
                       listener_conns[l_idx].port, 1, EMsg);

  if (sock < 0)
    {
    /* FAILURE */

    sprintf(tmpLine, "%s %d - port %d %s",
            msg_listnr_nocall,
            l_idx,
            listener_conns[l_idx].port,
            EMsg);

    /* we lost contact with the scheduler. reset*/
    listener_conns[l_idx].first_time = 1;
    log_err(errno, __func__, tmpLine);

    return(-1);
    }

  listener_conns[l_idx].first_time = 0;

  add_conn(
    sock,
    FromClientDIS,
    listener_conns[l_idx].address,
    listener_conns[l_idx].port,
    PBS_SOCK_INET,
    start_process_request);

  pthread_mutex_lock(svr_conn[sock].cn_mutex);
  svr_conn[sock].cn_authen = PBS_NET_CONN_FROM_PRIVIL;
  pthread_mutex_unlock(svr_conn[sock].cn_mutex);

  net_add_close_func(sock, listener_close);

  /* send command to Listener */
  pthread_mutex_lock(listener_command_mutex);

  if (put_4byte(sock, listener_command) < 0)
    {
    pthread_mutex_unlock(listener_command_mutex);

    sprintf(tmpLine, "%s %d - port %d",
            msg_listnr_nocall,
            l_idx + 1,
            listener_conns[l_idx].port);

    log_err(errno, __func__, tmpLine);

    close_conn(sock, FALSE);


    return(-1);
    }

  pthread_mutex_unlock(listener_command_mutex);

  sprintf(log_buf, msg_listnr_called, l_idx + 1,
    (listener_command != SCH_ERROR) ? PSchedCmdType[listener_command] : "ERROR");

  log_event(PBSEVENT_SCHED,PBS_EVENTCLASS_SERVER,server_name,log_buf);

  return (sock);
  }  /* END contact_listener() */


/*
 * notify_listeners - Send event message to any added listeners
 *
 */

void notify_listeners(void)

  {
  int  i;
  int  ret_sock;

  for (i = 0;i < MAXLISTENERS; i++)
    {
    /* do we have a listener in this slot?
     * if so then try to send data
     */

    if ((listener_conns[i].address != 0) && (listener_conns[i].sock == -1))
      {
      ret_sock = contact_listener(i);

      if (ret_sock != -1)
        {
        listener_conns[i].sock = ret_sock;
        }
      }
    }

  return;
  }  /* END notify_listeners() */




/*
 * scheduler_close - connection to scheduler has closed, clear scheduler_called
 */
void scheduler_close()

  {
  pthread_mutex_lock(scheduler_sock_jobct_mutex);
  scheduler_sock = -1;

  /*
   * This bit of code is intended to support the scheduler - server - mom
   * sequence.  A scheduler script may best written to run only one job per
   * cycle to ensure its newly taken resources are considered by the
   * scheduler before selecting another job.  In that case, rather than
   * wait a full cycle before scheduling the next job, we check for
   * one (and only one) job was run by the scheduler.  If true, then
   * recycle the scheduler.
   */

  if (scheduler_jobct == 1)
    {
    /* recycle the scheduler */

    pthread_mutex_lock(svr_do_schedule_mutex);
    svr_do_schedule = SCH_SCHEDULE_RECYC;
    pthread_mutex_unlock(svr_do_schedule_mutex);

    pthread_mutex_lock(listener_command_mutex);
    listener_command = SCH_SCHEDULE_RECYC;
    pthread_mutex_unlock(listener_command_mutex);
    }

  pthread_mutex_unlock(scheduler_sock_jobct_mutex);

  return;
  }  /* END scheduler_close() */


/*
 * listener_close - connection to listener has closed, clear listeners
 */

static void listener_close(

  int sock)

  {
  int  i;

  for (i = 0;i < MAXLISTENERS; i++)
    {
    /* Find matching listener for this socket
     * then clear out the socket
     */

    if (listener_conns[i].sock == sock)
      {
      listener_conns[i].sock = -1;
      }
    }

  return;
  }  /* END listener_close() */




/*
 * put_4byte() - write a 4 byte integer in network order
 *
 * Returns:  0 for sucess, -1 if error.
 */

static int put_4byte(

  int          sock, /* socket to read from */
  unsigned int val) /* 4 byte interger to write */

  {
  int amt;

  union
    {
    int unl;
    char unc[sizeof(unsigned int)];
    } un;

  un.unl = htonl(val);

  amt = write_ac_socket(sock, (char *)(un.unc + sizeof(unsigned int) - 4), 4);

  if (amt != 4)
    {
    /* FAILURE */

    return(-1);
    }

  /* SUCCESS */

  return(0);
  }


