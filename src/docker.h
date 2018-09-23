#ifndef DOCKER_H
#define DOCKER_H

/*
 * DOCKER_CONFIG_PATH_BUFFER_SIZE stores the max length of the path to the container json config file.
 * /var/lib/docker/containers/756535dc6e9ab9b560f84c85063f55952273a23192641fc2756aa9721d9d1000/config.v2.json = 106
 */
#define DOCKER_CONFIG_PATH_BUFFER_SIZE 128

/*
 * docker_container_get_name_from_id returns the name of the container having the given *full* id.
 * The returned container name *have* to be deallocated after use.
 * In case of failure, the function returns NULL.
 */
char *docker_get_container_name_from_id(const char *full_id);

#endif /* DOCKER_H */

