/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2017 Citronflexe
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#define SERVER_DEFAULT_MAX_CLIENTS	10

#define SERVER_BUFFER_SIZE	1024
#define SERVER_SOCKER_ERROR	-1

typedef struct {
  int sock;
} client_info;

typedef struct {
  int port_listening;
  int max_clients;
} server_info;

static int server_init(const int port_listening, const int max_clients)
{
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in sin = { 0 };

  if (sock == SERVER_SOCKER_ERROR) {
      perror("socket()");
      return SERVER_SOCKER_ERROR;
    }

  sin.sin_addr.s_addr = htonl(INADDR_ANY);
  sin.sin_port = htons(port_listening);
  sin.sin_family = AF_INET;

  if (bind(sock,(struct sockaddr *)&sin, sizeof(sin)) == SERVER_SOCKER_ERROR) {
      perror("bind()");
      return SERVER_SOCKER_ERROR;
    }

  if (listen(sock, max_clients) == SERVER_SOCKER_ERROR) {
      perror("listen()");
      return SERVER_SOCKER_ERROR;
    }

  return sock;
}

static void server_remove_client(client_info *clients, int *nb_sock,
				 int index_current_client)
{
  close(clients[index_current_client].sock);

  /* remove the client in the array */
  memmove(clients + index_current_client, clients + index_current_client + 1,
	  (*nb_sock - index_current_client - 1) * sizeof(client_info));
  --(*nb_sock);
}

static void server_send_all_client(client_info *clients, int *nb_sock,
				   const char *buf, const int buflen)
{
  int i = 0;

  while (i < *nb_sock) {
    if (write(clients[i].sock, buf, buflen) == SERVER_SOCKER_ERROR) {
      perror("send()");
      server_remove_client(clients, nb_sock, i);
    }
    else {
      ++i;
    }
  }
}

static void server_new_client_data(client_info *clients, int *nb_sock,
				   int index_current_client)
{
  char buf[SERVER_BUFFER_SIZE] = {0};
  int  buflen = 0;

  if ((buflen = read(clients[index_current_client].sock, buf,
		     sizeof(buf)-1)) <= 0) {
    server_remove_client(clients, nb_sock, index_current_client);
    return;
  }

  printf("[id:%d] %s - (%d) <%.*s>\n", clients[index_current_client].sock,
	 __FUNCTION__, buflen, buflen, buf);

  server_send_all_client(clients, nb_sock, buf, buflen);
}

static int server_new_client_connect(client_info *clients, int *nb_sock,
				     const int sock)
{
  struct sockaddr_in csin = { 0 };
  socklen_t sinsize = sizeof(csin);
  int csock = accept(sock, (struct sockaddr *)&csin, &sinsize);

  if (csock == SERVER_SOCKER_ERROR) {
    perror("accept()");
  }
  printf("[id:%d] %s\n", csock, __FUNCTION__);

  clients[*nb_sock].sock = csock;
  ++(*nb_sock);
  return csock;
}

static void server_select(const int sock, const int max_clients)
{
  int i = 0;
  int csock = 0;
  int nb_sock = 0;
  int max_sock = sock;
  client_info clients[max_clients];
  fd_set rdfs;

  while (1) {
    FD_ZERO(&rdfs);
    FD_SET(sock, &rdfs);

    i = 0;
    while (i < nb_sock) {
      FD_SET(clients[i].sock, &rdfs);
      ++i;
    }

    if (select(max_sock + 1, &rdfs, NULL, NULL, NULL) == SERVER_SOCKER_ERROR) {
      perror("select()");
      exit(errno);
    }

    if (FD_ISSET(sock, &rdfs)) {
      csock = server_new_client_connect(clients, &nb_sock, sock);
      if (csock == SERVER_SOCKER_ERROR) {
	return;
      }
      max_sock = csock > max_sock ? csock : max_sock;
    }
    else {
      i = 0;
      while (i < nb_sock) {
	if (FD_ISSET(clients[i].sock, &rdfs)) {
	  server_new_client_data(clients, &nb_sock, i);
	}
	++i;
      }
    }
  }
}

static void server_usage(const char *program_name)
{
  printf("%s -p port -c [max-clients [default=%d]]\n",
	 program_name, SERVER_DEFAULT_MAX_CLIENTS);
}

static int server_param(int argc, char **argv, server_info *server)
{
  int opt;

  const struct option long_options[] = {
      {"port",        required_argument, NULL, 'p'},
      {"max-clients", optional_argument, NULL, 'c'},
      {NULL, 0, NULL, 0}
    };

  while ((opt = getopt_long(argc, argv, "p:c::", long_options, NULL)) != -1) {
    switch (opt) {
    case 'p':
      server->port_listening = atoi(optarg);
      break;
    case 'c':
      if (optarg) {
	server->max_clients = atoi(optarg);
      }
      else {
	if (argv[optind][0] != '-') {
	  server->max_clients = atoi(argv[optind]);
	}
      }
      break;
    default:
      server_usage(argv[0]);
      return 1;
    }
  }

  if (server->port_listening <= 0) {
    server_usage(argv[0]);
    return 1;
  }

  if (server->max_clients <= 0) {
    server->max_clients = SERVER_DEFAULT_MAX_CLIENTS;
  }

  return 0;
}

int main(int argc, char **argv)
{
  server_info server = {0};
  int sock;

  if (server_param(argc, argv, &server)) {
      printf("%d %d\n", server.port_listening, server.max_clients);
    return 0;
  }
  printf("%d %d\n", server.port_listening, server.max_clients);
  sock = server_init(server.port_listening, server.max_clients);

  if (sock != SERVER_SOCKER_ERROR) {
    printf("The server is running with socket %d on port %d\n",
	   sock, server.port_listening);
    printf("The server is limited to %d simultaneous connections\n",
	   server.max_clients);
    server_select(sock, server.max_clients);
  }
  return 0;
}
