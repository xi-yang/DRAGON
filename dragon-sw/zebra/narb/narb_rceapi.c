#include "narb_rceapi.h"
#include <log.h>

int rce_api_sock = -1;
char * RCE_HOST_ADDR = NULL;
int RCE_API_PORT = 2678;

struct rce_api_msg * rce_api_msg_new (u_char msgtype, u_char action, void *msgbody, u_int32_t ucid, u_int32_t seqnum, u_int16_t msglen)
{
    struct rce_api_msg *msg;

    msg = (struct rce_api_msg *)malloc(sizeof (struct rce_api_msg));
    memset (msg, 0, sizeof(struct rce_api_msg));

    msg->hdr.msgtype = msgtype;
    msg->hdr.action = action;
    msg->hdr.msglen = htons (msglen);
    msg->hdr.ucid = htonl (ucid);
    msg->hdr.msgseq = htonl (seqnum);
    msg->hdr.chksum = MSG_CHKSUM(msg->hdr);
   
    msg->body = malloc(msglen);
    memcpy(msg->body, msgbody, msglen);

    return msg;
}

void rce_api_msg_delete (struct rce_api_msg* msg)
{
    assert(msg);
    assert(msg->body);    	

    free(msg->body);
    free(msg);
}


int narb_rceapi_connect (char *host, int port)
{
    struct sockaddr_in addr;
    struct hostent *hp;
    int fd;
    int ret;
    int size;
    int on = 1;
	
    hp = gethostbyname (host);
      if (!hp)
    {
      fprintf (stderr, "ospf_apiclient_connect: no such host %s\n", host);
      return (-1);
    }

    fd = socket (AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
      {
	  zlog_warn("narb_connect_peer: socket(): %s", strerror (errno));
	  return (-1);
      }

                                                                                
  /* Reuse addr and port */
  ret = setsockopt (fd, SOL_SOCKET,
                    SO_REUSEADDR, (void *) &on, sizeof (on));
  if (ret < 0)
    {
      fprintf (stderr, "narb_rceapi_connect: SO_REUSEADDR failed\n");
      close (fd);
      return (-1);
    }

#ifdef SO_REUSEPORT
  ret = setsockopt (fd, SOL_SOCKET, SO_REUSEPORT,
                    (void *) &on, sizeof (on));
  if (ret < 0)
    {
      fprintf (stderr, "narb_rceapi_connect: SO_REUSEPORT failed\n");
      close (fd);
      return (-1);
    }
#endif /* SO_REUSEPORT */

  memset (&addr, 0, sizeof (struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_port = htons (port+1000);
  //addr.sin_len = sizeof (struct sockaddr_in);
  size = sizeof (struct sockaddr_in);
                                                                                
  ret = bind (fd, (struct sockaddr *) &addr, size);
  if (ret < 0)
    {
      fprintf (stderr, "narb_rceapi_connect: bind sync socket failed\n");
      close (fd);
      return (-1);
    }
                                                                                
  /* Prepare address structure for connect */
  memcpy (&addr.sin_addr, hp->h_addr, hp->h_length);
  addr.sin_family = AF_INET;
  addr.sin_port = htons (port);
  //addr.sin_len = sizeof (struct sockaddr_in);
 
  /* Now establish synchronous channel with OSPF daemon */
  ret = connect (fd, (struct sockaddr *) &addr,
                 sizeof (struct sockaddr_in));
  if (ret < 0)
    {
      zlog_warn("narb_rceapi_connect: connect(): %s", strerror (errno));
      close (fd);
      return (-1);
    }

  return fd;

}


struct rce_api_msg * narb_rceapi_read (int fd)
{
  static char buf[API_MAX_MSG_SIZE];
  int ret;
  struct rce_api_msg_header hdr;
  int bodylen;
  int rlen;
  struct rce_api_msg *msg;
  
  /* Read message header */
  rlen = readn (fd, (char *) &hdr, sizeof (struct rce_api_msg_header));

  if (rlen < 0)
    {
      zlog_warn("RCE APIReader failed to read from %d\n", fd);
      return NULL;
    }
  else if (rlen == 0)
    {
      zlog_warn("Connection closed for APIReader(%d)\n", fd);
      return NULL;    }
  else if (rlen != sizeof (struct rce_api_msg_header))
    {
      zlog_warn("RCE APIReader(%d) cannot read the message header\n", fd);
      return NULL;
    }

  if (MSG_CHKSUM(hdr) != hdr.chksum)
  {
      zlog_warn("RCE APIReader(%d) packet corrupt \n", fd);
      return NULL;
  }

  /* Determine body length. */
  bodylen = ntohs (hdr.msglen);
  if (bodylen > API_MAX_MSG_SIZE)
  {
      zlog_warn("RCE APIReader(%d) cannot read oversized packet\n", fd);
      return NULL;
  }
    
  if (bodylen > 0)
    {
      /* Read message body*/
      rlen = readn (fd, buf, bodylen);
      if (rlen < 0)
	{
	   zlog_warn("RCE APIReader failed to read from %d\n", fd);
          return NULL;
	}
      else if (rlen == 0)
	{
	   zlog_warn("Connection closed for APIReader(%d)\n", fd);
          return NULL;
	}
      else if (rlen != bodylen)
       {
         zlog_warn("APIReader(%d) cannot read the message body. \n", fd);
         return NULL;
	}
    }

  /* Allocate new message*/
  msg = rce_api_msg_new (hdr.msgtype, hdr.action, buf, ntohl (hdr.ucid), ntohl (hdr.msgseq), ntohs (hdr.msglen));

  return msg;
}

int narb_rceapi_send (int fd, struct rce_api_msg *msg)
{
  static char buf[API_MAX_MSG_SIZE];
  int len, wlen;
  int ret = 0;

  assert (fd > 0);  
  assert (msg);
  assert (msg->body);

  /* Length of message including header*/
   len = sizeof (struct rce_api_msg_header) + ntohs (msg->hdr.msglen);

  /* Make contiguous memory buffer for message */
  memcpy (buf, &msg->hdr, sizeof (struct rce_api_msg_header));
  memcpy (buf + sizeof (struct rce_api_msg_header), msg->body, ntohs (msg->hdr.msglen));

  if (MSG_CHKSUM(msg->hdr) != msg->hdr.chksum)
  {
      zlog_warn("RCE APIWriter(%d) packet corrupt\n", fd);
      return -1;
  }

  wlen = writen(fd, buf, len);
  if (wlen < 0)
    {
      zlog_warn("RCE APIWriter failed to write %d\n",fd);
      return -1;
    }
  else if (wlen == 0)
    {
      zlog_warn("Connection closed for APIWriter(%d)\n", fd);
      return -1;
    }
  else if (wlen != len)
    {
      zlog_warn("RCE APIWriter(%d) cannot write the message\n", fd);
      return -1;
    }

  return ret;
}

int narb_rceapi_msghandle (struct rce_api_msg*msg)
{
    u_int32_t ack_id;
    switch (msg->hdr.msgtype)
    {
    case MSG_LSA:
        switch (msg->hdr.action)
        {
        case ACT_ACK:
            ack_id = *((u_int32_t *)msg->body);
            zlog_info("LSA update acklowdged with id %d", ack_id);
	     break;
        default:
             break;
        }
        break;
     case MSG_LSP:
        if (msg->hdr.action == ACT_ERROR)
            zlog_info("LSP query return error code %d", *(int*)(msg->body));
        else if (msg->hdr.action == ACT_ACKDATA)
            zlog_info("LSP query return ERO with %d hops", (ntohs(msg->hdr.msglen) - 4)/4);
        break;
     case MSG_AAA:
        break;
     case MSG_RM:
        break;
    default:
        zlog_err("Unkonwn msg type %d \n", msg->hdr.msgtype);
    }
    return 0;
}

