/*
 *  Copyright (c) 2018, INRIA
 *  Copyright (c) 2018, University of Lille
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <czmq.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <bson.h>

#include "config.h"
#include "events.h"
#include "storage.h"


#define DEFAULT_CGROUP_BASEPATH "/sys/fs/cgroup/perf_event"

struct config *
config_create(void)
{
    struct config *config = malloc(sizeof(struct config));

    if (!config)
	return NULL;

    /* sensor default config */
    config->sensor.verbose = 0;
    config->sensor.frequency = 1000;
    config->sensor.cgroup_basepath = DEFAULT_CGROUP_BASEPATH;
    config->sensor.name = NULL;

    /* storage default config */
    config->storage.type = STORAGE_CSV;
    config->storage.U_flag = NULL;
    config->storage.D_flag = NULL;
    config->storage.C_flag = NULL;

    /* events default config */
    config->events.system = NULL;
    config->events.containers = NULL;

    return config;
}

int
parse_config_file_path(int argc, char **argv, char ** config_file_path)
{
  *config_file_path = NULL;
  for(int i = 0; i < argc; i++){
    char * arg = argv[i];
    if(strcmp(arg, "--config-file") == 0){
      if(i + 1 >= argc){
	zsys_error("config: file path needed after --config-file");
	return -1;
      }
      *config_file_path = argv[i + 1];
      return 0;

    }
  }
  return 0;
}

static int
parse_frequency(const char *str, unsigned int *frequency)
{
    unsigned long value;
    char *str_endp = NULL;

    errno = 0;
    value = strtoul(str, &str_endp, 0);

    /* check if the string have been fully processed */
    if (str == str_endp || *str_endp != '\0' || errno != 0) {
        return -1;
    }

    /* check if the extracted value fit in the destination type before casting */
    if (value > UINT_MAX) {
	return -1;
    }

    *frequency = (unsigned int)value;
    return 0;
}

static int
parse_event_array(bson_iter_t *iter, struct events_group *current_events_group)
{
  while(bson_iter_next(iter)){
    switch(bson_iter_type(iter)){
    case BSON_TYPE_UTF8:
      if (!current_events_group) {
	zsys_error("config: you cannot add an event to an inexisting events group");
	return -1;
      }
      if (events_group_append_event(current_events_group, bson_iter_utf8(iter, NULL))) {
	zsys_error("config: event '%s' is invalid or unsupported by this machine", bson_iter_utf8(iter, NULL));
	return -1;
      }
      break;
    default:
      zsys_error("config: an event name, in event group %s, is not a string", current_events_group->name);
      return -1;
    }
  }
  return 0;
}

static int
iter_on_event_group_config(bson_iter_t * iter, struct config *config, char *group_type)
{
  const char * group_name;
  struct events_group *current_events_group = NULL;
  bson_iter_t child_iter;
  bson_iter_t event_array_iter;
  const char * key_name;

  while(bson_iter_next(iter)){
    if(bson_iter_type(iter) != BSON_TYPE_DOCUMENT){
      zsys_error("config: you must provide an event group name for %s events", group_type);
      return -1;
    }
    group_name = bson_iter_key(iter);
    current_events_group = events_group_create(group_name);
    if (!current_events_group) {
      zsys_error("config: failed to create %s events group", group_type);
      return -1;
    }
    if(strcmp(group_type, "system") == 0){
      zhashx_insert(config->events.system, group_name, current_events_group);
      events_group_destroy(&current_events_group);
      current_events_group = zhashx_lookup(config->events.system, group_name); /* get the duplicated events group */
    }
    else{
      zhashx_insert(config->events.containers, group_name, current_events_group);
      events_group_destroy(&current_events_group);
      current_events_group = zhashx_lookup(config->events.containers, group_name); /* get the duplicated events group */
    }

    bson_iter_recurse(iter, &child_iter);
    while(bson_iter_next (&child_iter)){
      key_name = bson_iter_key(&child_iter);
      switch(bson_iter_type(&child_iter)){
      case BSON_TYPE_UTF8:
	if(strcmp(key_name, "monitoring_type") == 0){
	  if(strcmp(bson_iter_utf8(&child_iter, NULL), "MONITOR_ONE_CPU_PER_SOCKET") == 0){
	    current_events_group->type = MONITOR_ONE_CPU_PER_SOCKET;
	    break;
	  }
	  if(strcmp(bson_iter_utf8(&child_iter, NULL), "MONITOR_ALL_CPU_PER_SOCKET") == 0){
	    current_events_group->type = MONITOR_ALL_CPU_PER_SOCKET;
	    break;
	  }
	  zsys_error("config: unknow value %s for monitoring_type", bson_iter_utf8(&child_iter, NULL));
	  return -1;
	}
	zsys_error("config: unknow config option %s in event sub section", key_name);
	return -1;
      case BSON_TYPE_ARRAY:
	bson_iter_recurse (&child_iter, &event_array_iter);
	if(parse_event_array(&event_array_iter, current_events_group))
	  return -1;
	break;
      default:
	zsys_error("config: unknow type for : %s", key_name);
	return -1;
      }
    }
  }
  return 0;
}

static int
iter_on_storage_config(bson_iter_t *iter, struct config *config)
{
  const char * key_name;

  while(bson_iter_next (iter)){
    key_name = bson_iter_key(iter);
    switch(bson_iter_type(iter)){
      case BSON_TYPE_UTF8:
	if(strcmp(key_name, "type") == 0){
	  config->storage.type = storage_module_get_type(bson_iter_utf8(iter, NULL));
	  if (config->storage.type == STORAGE_UNKNOWN) {
	    zsys_error("config: storage module '%s' is invalid or disabled at compile time", bson_iter_utf8(iter, NULL));
	    return -1;
	  }
	  break;
	}
	if(strcmp(key_name, "database") == 0){
	  config->storage.D_flag = bson_iter_utf8(iter, NULL);
	  break;
	}
	if(strcmp(key_name, "uri") == 0 || strcmp(key_name, "directory") == 0){
	  config->storage.U_flag = bson_iter_utf8(iter, NULL);
	  break;
	}
	if(strcmp(key_name, "collection") == 0){
	  config->storage.C_flag = bson_iter_utf8(iter, NULL);
	  break;
	}
	zsys_error("config: unknow string config option %s in storage sub section", key_name);
	return -1;
    case BSON_TYPE_INT32:
      config->storage.P_flag = bson_iter_int32(iter);
      break;
    default:
      zsys_error("config: unknow config option %s in storage sub section", key_name);
      return -1;
    }
  }
  return 0;
}

static int
iter_on_root_config(bson_iter_t * iter, struct config *config)
{
  const char * key_name;
  bson_iter_t child_iter;

  while(bson_iter_next (iter)){
    key_name = bson_iter_key(iter);
    switch(bson_iter_type(iter)){
    case BSON_TYPE_BOOL:
      if(strcmp(key_name, "verbose") == 0){
	config->sensor.verbose++;
	break;
      }
      zsys_error("config: invalid boolean value for %s", key_name);
      return -1;
    case BSON_TYPE_INT32:
      if(strcmp(key_name, "frequency") == 0){
	config->sensor.frequency = bson_iter_int32(iter);
	break;
      }
      zsys_error("config: invalid integer value for %s", key_name);
      return -1;
    case BSON_TYPE_UTF8:
      if(strcmp(key_name, "cgroup_basepath") == 0){
	config->sensor.cgroup_basepath = bson_iter_utf8(iter, NULL);
	break;
      }
      else if(strcmp(key_name, "name") == 0){
	config->sensor.name = bson_iter_utf8(iter, NULL);
	break;
      }
      zsys_error("config: invalid string value for %s", key_name);
      return -1;
    case BSON_TYPE_DOCUMENT:
      bson_iter_recurse(iter, &child_iter);
      if(strcmp(key_name, "system") == 0){
	if(iter_on_event_group_config(&child_iter, config, "system"))
	  return -1;
	break;
      }
      if(strcmp(key_name, "container") == 0){
	if(iter_on_event_group_config(&child_iter, config, "container"))
	  return -1;
	break;
      }
      if(strcmp(key_name, "output") == 0){
	if(iter_on_storage_config(&child_iter, config))
	  return -1;
	break;
      }
      zsys_error("config: invalid sub category %s", key_name);
      return -1;
    default:
      zsys_error("config: invalid value for %s", key_name);
      return -1;
    }
  }
  return 0;

}

int
config_setup_from_file(struct config *config, bson_t * doc)
{

  bson_iter_t iter;

  /* stores events to monitor globally (system) */
  config->events.system = zhashx_new();
  if (!config->events.system) {
    zsys_error("config: failed to create system events group container");
    return -1;
  }
  zhashx_set_duplicator(config->events.system, (zhashx_duplicator_fn *) events_group_dup);
  zhashx_set_destructor(config->events.system, (zhashx_destructor_fn *) events_group_destroy);

  /* stores events to monitor per-container */
  config->events.containers = zhashx_new();
  if (!config->events.containers) {
    zsys_error("config: failed to create containers events group container");
    return -1;
  }
  zhashx_set_duplicator(config->events.containers, (zhashx_duplicator_fn *) events_group_dup);
  zhashx_set_destructor(config->events.containers, (zhashx_destructor_fn *) events_group_destroy);


  if(!bson_iter_init (&iter, doc)){
    fprintf(stderr, "failed to init iterator");
    return -1;
  }
  return iter_on_root_config(&iter, config);

}

static void
print_usage(void)
{
    // TODO: write an usage text
}

int
config_setup_from_cli(int argc, char **argv, struct config *config)
{
    int ret = -1;
    int c;
    struct events_group *current_events_group = NULL;

    /* stores events to monitor globally (system) */
    config->events.system = zhashx_new();
    if (!config->events.system) {
	zsys_error("config: failed to create system events group container");
	goto end;
    }
    zhashx_set_duplicator(config->events.system, (zhashx_duplicator_fn *) events_group_dup);
    zhashx_set_destructor(config->events.system, (zhashx_destructor_fn *) events_group_destroy);

    /* stores events to monitor per-container */
    config->events.containers = zhashx_new();
    if (!config->events.containers) {
	zsys_error("config: failed to create containers events group container");
	goto end;
    }
    zhashx_set_duplicator(config->events.containers, (zhashx_duplicator_fn *) events_group_dup);
    zhashx_set_destructor(config->events.containers, (zhashx_destructor_fn *) events_group_destroy);

    while ((c = getopt(argc, argv, "vf:p:n:s:c:e:or:U:D:C:P:")) != -1) {
	switch (c) {
	    case 'v':
		config->sensor.verbose++;
		break;
	    case 'f':
		if(parse_frequency(optarg, &config->sensor.frequency)) {
		    zsys_error("config: the given frequency is invalid or out of range");
		    goto end;
		}
		break;
	    case 'p':
		config->sensor.cgroup_basepath = optarg;
		break;
	    case 'n':
		config->sensor.name = optarg;
		break;
	    case 's':
		current_events_group = events_group_create(optarg);
		if (!current_events_group) {
		    zsys_error("config: failed to create system events group");
		    goto end;
		}
		zhashx_insert(config->events.system, optarg, current_events_group);
		events_group_destroy(&current_events_group);
		current_events_group = zhashx_lookup(config->events.system, optarg); /* get the duplicated events group */
		break;
	    case 'c':
		current_events_group = events_group_create(optarg);
		if (!current_events_group) {
		    zsys_error("config: failed to create containers events group");
		    goto end;
		}
		zhashx_insert(config->events.containers, optarg, current_events_group);
		events_group_destroy(&current_events_group);
		current_events_group = zhashx_lookup(config->events.containers, optarg); /* get the duplicated events group */
		break;
	    case 'o':
		if (!current_events_group) {
		    zsys_error("config: you cannot set the type of an inexistent events group");
		    goto end;
		}
		current_events_group->type = MONITOR_ONE_CPU_PER_SOCKET;
		break;
	    case 'e':
		if (!current_events_group) {
		    zsys_error("config: you cannot add an event to an inexisting events group");
		    goto end;
		}
		if (events_group_append_event(current_events_group, optarg)) {
		    zsys_error("config: event '%s' is invalid or unsupported by this machine", optarg);
		    goto end;
		}
		break;
	    case 'r':
		config->storage.type = storage_module_get_type(optarg);
		if (config->storage.type == STORAGE_UNKNOWN) {
		    zsys_error("config: storage module '%s' is invalid or disabled at compile time", optarg);
		    goto end;
		}
		break;
	    case 'U':
		config->storage.U_flag = optarg;
		break;
	    case 'D':
		config->storage.D_flag = optarg;
		break;
	    case 'C':
		config->storage.C_flag = optarg;
		break;
	    case 'P':
	      config->storage.P_flag = (int)strtol(optarg, NULL, 10);
		break;
	    default:
		print_usage();
		goto end;
	}
    }
    ret = 0;

end:
    return ret;
}

int
config_validate(struct config *config)
{
    const struct config_sensor *sensor = &config->sensor;
    const struct config_storage *storage = &config->storage;
    const struct config_events *events = &config->events;

    if (!sensor->name) {
	zsys_info("config: you must provide a sensor name");
	return -1;
    }

    if (zhashx_size(events->system) == 0 && zhashx_size(events->containers) == 0) {
	zsys_error("config: you must provide event(s) to monitor");
	return -1;
    }

    if (storage->type == STORAGE_CSV && (!storage->U_flag)) {
	zsys_error("config: the CSV storage module requires the 'U' flag to be set");
	return -1;
    }

    if (storage->type == STORAGE_SOCKET && (!storage->U_flag || !storage->P_flag)) {
	zsys_error("config: the socket storage module requires the 'U' and 'P' flags to be set");
	return -1;
    }

    if(storage->type == STORAGE_SOCKET && config->storage.P_flag == 0){
      zsys_error("config: %d is not a valid port number", config->storage.P_flag);
      return -1;
    }

#ifdef HAVE_MONGODB
    if (storage->type == STORAGE_MONGODB && (!storage->U_flag || !storage->D_flag || !storage->C_flag)) {
	zsys_error("config: the MongoDB storage module requires the 'U', 'D' and 'C' flags to be set");
	return -1;
    }
#endif

    return 0;
}

void
config_destroy(struct config *config)
{
    if (!config)
	return;

    zhashx_destroy(&config->events.containers);
    zhashx_destroy(&config->events.system);

    free(config);
}
