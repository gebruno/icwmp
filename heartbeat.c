#include <pthread.h>
#include <unistd.h>

#include "heartbeat.h"
#include "common.h"
#include "config.h"
#include "session.h"
#include "cwmp_uci.h"
#include "backupSession.h"
#include "log.h"
#include "event.h"
#include "http.h"

pthread_cond_t threshold_heartbeat_session;
pthread_cond_t threasheld_retry_session;
pthread_mutex_t mutex_heartbeat;
pthread_mutex_t mutex_heartbeat_session;
bool old_heartbeat_enable = false;
int heart_beat_retry_count_session = 0;

static struct session_status heart_beat_session_status = {0};

void check_trigger_heartbeat_session()
{
	bool enable = global_bool_param_read(&cwmp_main.conf.heart_beat_enable);
	if (enable && !old_heartbeat_enable)
		pthread_cond_signal(&threshold_heartbeat_session);
}

int add_heart_beat_event(struct session *heartbeat_session)
{
	struct event_container *event_container;
	event_container = calloc(1, sizeof(struct event_container));
	if (event_container == NULL) {
		CWMP_LOG(ERROR, "heartbeat %s: event_container is null", __FUNCTION__);
		return -1;
	}
	INIT_LIST_HEAD(&(event_container->head_dm_parameter));
	list_add(&(event_container->list), heartbeat_session->head_event_container.prev);
	event_container->code = EVENT_IDX_14HEARTBEAT;
	event_container->command_key = strdup("");
	event_container->id = 1;
	/*
	 * event_container will be freed in the destruction of the session heartbeat_session
	 */
	// cppcheck-suppress memleak
	return 0;
}

void *thread_heartbeat_session(void *v __attribute__((unused)))
{
	static struct timespec heartbeat_interval = { 0, 0 };

	sleep(2);
	for (;;) {
		if (thread_end)
			break;

		bool enable = global_bool_param_read(&cwmp_main.conf.heart_beat_enable);
		if (enable) {
			heartbeat_interval.tv_sec = time(NULL) + global_int_param_read(&cwmp_main.conf.heartbeat_interval);
			pthread_mutex_lock(&mutex_heartbeat);
			pthread_cond_timedwait(&threshold_heartbeat_session, &mutex_heartbeat, &heartbeat_interval);
			if (thread_end)
				break;

			if (cwmp_main.session_status.last_status == SESSION_FAILURE) {
				CWMP_LOG(WARNING, "Not able to start HEARTBEAT Session for this period: CWMP Session is retrying");
				pthread_cond_wait(&threasheld_retry_session, &mutex_heartbeat);
				//continue;
			}

			if (thread_end)
				break;

			pthread_mutex_lock(&mutex_heartbeat_session);
			struct session *heartbeat_session = NULL;
			heartbeat_session = calloc(1, sizeof(struct session));
			if (heartbeat_session == NULL) {
				pthread_mutex_unlock(&mutex_heartbeat_session);
				pthread_mutex_unlock(&mutex_heartbeat);
				continue;
			}
			INIT_LIST_HEAD(&(heartbeat_session->head_event_container));
			INIT_LIST_HEAD(&(heartbeat_session->head_rpc_acs));
			INIT_LIST_HEAD(&(heartbeat_session->head_rpc_cpe));
			struct rpc *rpc_acs;
			rpc_acs = cwmp_add_session_rpc_acs_head(heartbeat_session, RPC_ACS_INFORM);
			if (rpc_acs == NULL) {
				pthread_mutex_lock(&(cwmp_main.mutex_session_queue));
				cwmp_session_destructor(heartbeat_session);
				pthread_mutex_unlock(&(cwmp_main.mutex_session_queue));
				pthread_mutex_unlock(&mutex_heartbeat_session);
				pthread_mutex_unlock(&mutex_heartbeat);
				continue;
			}
			if (add_heart_beat_event(heartbeat_session) != 0) {
				pthread_mutex_lock(&(cwmp_main.mutex_session_queue));
				cwmp_session_destructor(heartbeat_session);
				pthread_mutex_unlock(&(cwmp_main.mutex_session_queue));
				pthread_mutex_unlock(&mutex_heartbeat_session);
				pthread_mutex_unlock(&mutex_heartbeat);
				continue;
			}

			if (heart_beat_session_status.last_status == SESSION_FAILURE) {
				cwmp_config_load(&cwmp_main);
				if (thread_end) {
					pthread_mutex_lock(&(cwmp_main.mutex_session_queue));
					cwmp_session_destructor(heartbeat_session);
					pthread_mutex_unlock(&(cwmp_main.mutex_session_queue));
					pthread_mutex_unlock(&mutex_heartbeat_session);
					pthread_mutex_unlock(&mutex_heartbeat);
					continue;
				}
			}

			heart_beat_session_status.last_end_time = 0;
			heart_beat_session_status.last_start_time = time(NULL);
			heart_beat_session_status.last_status = SESSION_RUNNING;
			heart_beat_session_status.next_retry = 0;

			if (file_exists(fc_cookies))
				remove(fc_cookies);

			CWMP_LOG(INFO, "Start HEARTBEAT session");
			int error = cwmp_schedule_rpc(&cwmp_main, heartbeat_session);
			CWMP_LOG(INFO, "End HEARTBEAT session");

			if (thread_end) {
				event_remove_all_event_container(heartbeat_session, RPC_SEND);
				run_session_end_func();
				pthread_mutex_lock(&(cwmp_main.mutex_session_queue));
				cwmp_session_destructor(heartbeat_session);
				pthread_mutex_unlock(&(cwmp_main.mutex_session_queue));
				pthread_mutex_unlock(&(cwmp_main.mutex_session_send));
				pthread_mutex_unlock(&mutex_heartbeat);
				// Exiting to avoid race conditions
				exit(0);
			}

			if (error || heartbeat_session->error == CWMP_RETRY_SESSION) {
				cwmp_config_load(&cwmp_main);
				heart_beat_retry_count_session++;
				run_session_end_func();
				CWMP_LOG(INFO, "Retry HEARTBEAT session, retry count = %d, retry in %ds", cwmp_main.retry_count_session, cwmp_get_retry_interval(&cwmp_main, 1));
				heart_beat_session_status.last_end_time = time(NULL);
				heart_beat_session_status.last_status = SESSION_FAILURE;
				heart_beat_session_status.next_retry = time(NULL) + cwmp_get_retry_interval(&cwmp_main, 1);
				heartbeat_interval.tv_sec = time(NULL) + cwmp_get_retry_interval(&cwmp_main, 1);
				heart_beat_session_status.failure_session++;
				pthread_mutex_unlock(&mutex_heartbeat_session);
				pthread_mutex_unlock(&mutex_heartbeat);
				continue;
			}
			event_remove_all_event_container(heartbeat_session, RPC_SEND);
			run_session_end_func();
			pthread_mutex_lock(&(cwmp_main.mutex_session_queue));
			cwmp_session_destructor(heartbeat_session);
			pthread_mutex_unlock(&(cwmp_main.mutex_session_queue));
			heart_beat_retry_count_session = 0;
			heart_beat_session_status.last_end_time = time(NULL);
			heart_beat_session_status.last_status = SESSION_SUCCESS;
			heart_beat_session_status.next_retry = 0;
			heart_beat_session_status.success_session++;
			heartbeat_interval.tv_sec = time(NULL) + global_int_param_read(&cwmp_main.conf.heartbeat_interval);
			pthread_mutex_unlock(&mutex_heartbeat_session);
			pthread_mutex_unlock(&mutex_heartbeat);
		} else {
			pthread_mutex_lock(&mutex_heartbeat);
			pthread_cond_wait(&threshold_heartbeat_session, &mutex_heartbeat);
			pthread_mutex_unlock(&mutex_heartbeat);
		}
	}
	return NULL;
}
