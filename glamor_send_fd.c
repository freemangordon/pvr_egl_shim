/*
 * glamor_send_fd.c
 *
 * Copyright (C) 2020 Ivaylo Dimitrov <ivo.g.dimitrov.75@gmail.com>
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/un.h>

#include "glamor_test.h"

ssize_t
write_fd(int sock, void *buf, ssize_t buflen, int fd)
{
  ssize_t		size;
  struct msghdr	msg;
  struct iovec	iov;
  union {
    struct cmsghdr cmsghdr;
    char control[CMSG_SPACE(sizeof (int))];
  } cmsgu;
  struct cmsghdr *cmsg;

  iov.iov_base = buf;
  iov.iov_len = buflen;

  msg.msg_name = NULL;
  msg.msg_namelen = 0;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  if (fd != -1)
  {
    msg.msg_control = cmsgu.control;
    msg.msg_controllen = sizeof(cmsgu.control);

    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_len = CMSG_LEN(sizeof (int));
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;

    *((int *) CMSG_DATA(cmsg)) = fd;
  }
  else
  {
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
  }

  size = sendmsg(sock, &msg, 0);

  if (size < 0)
    perror ("sendmsg");

  return size;
}

ssize_t
read_fd(int sock, void *buf, ssize_t bufsize, int *fd)
{
  ssize_t		size;

  if (fd) {
    struct msghdr	msg;
    struct iovec	iov;
    union {
      struct cmsghdr	cmsghdr;
      char		control[CMSG_SPACE(sizeof (int))];
    } cmsgu;
    struct cmsghdr	*cmsg;

    iov.iov_base = buf;
    iov.iov_len = bufsize;

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsgu.control;
    msg.msg_controllen = sizeof(cmsgu.control);
    size = recvmsg (sock, &msg, 0);

    if (size < 0)
    {
      perror ("recvmsg");
      return 0;
    }

    cmsg = CMSG_FIRSTHDR(&msg);

    if (cmsg && cmsg->cmsg_len == CMSG_LEN(sizeof(int)))
    {
      if (cmsg->cmsg_level != SOL_SOCKET) {
        fprintf (stderr, "invalid cmsg_level %d\n",
                 cmsg->cmsg_level);
        return 0;
      }

      if (cmsg->cmsg_type != SCM_RIGHTS)
      {
        fprintf (stderr, "invalid cmsg_type %d\n",
                 cmsg->cmsg_type);
        exit(1);
      }

      *fd = *((int *) CMSG_DATA(cmsg));
      fprintf (stderr, "received fd %d\n", *fd);
    }
    else
      *fd = -1;
  }
  else
  {
    size = read (sock, buf, bufsize);

    if (size < 0)
    {
      perror("read");
      return 0;
    }
  }
  return size;
}

int
bind_named_socket(const char *filename)
{
  struct sockaddr_un name;
  int sock;

  /* Create the socket. */
  sock = socket(PF_LOCAL, SOCK_STREAM, 0);

  if (sock < 0)
  {
    perror ("socket");
    return -1;
  }

  /* Bind a name to the socket. */
  name.sun_family = AF_LOCAL;
  strncpy (name.sun_path, filename, sizeof (name.sun_path));
  name.sun_path[sizeof (name.sun_path) - 1] = '\0';

  if (bind (sock, (struct sockaddr *)&name, SUN_LEN(&name)) < 0)
  {
    perror ("bind");
    return -1;
  }

  return sock;
}

int
connect_named_socket(const char *filename)
{
  struct sockaddr_un name;
  int sock;

  /* Create the socket. */
  sock = socket(PF_LOCAL, SOCK_STREAM, 0);

  if (sock < 0)
  {
    perror ("socket");
    return -1;
  }

  /* Bind a name to the socket. */
  name.sun_family = AF_LOCAL;
  strncpy (name.sun_path, filename, sizeof (name.sun_path));
  name.sun_path[sizeof (name.sun_path) - 1] = '\0';

  if (connect (sock, (struct sockaddr *)&name, SUN_LEN(&name)) != 0)
  {
    perror ("connect");
    return -1;
  }

  return sock;
}
