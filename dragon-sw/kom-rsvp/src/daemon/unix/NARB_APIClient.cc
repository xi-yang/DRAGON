/****************************************************************************

NARB Client Interface Module header file
Created by Xi Yang @ 03/15/2006
To be incorporated into KOM-RSVP-TE package

****************************************************************************/
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>
#include "RSVP_ProtocolObjects.h"
#include "NARB_APIClient.h"

int readn (int fd, char *ptr, int nbytes);
int writen(int fd, char *ptr, int nbytes);

NARB_APIClient NARB_APIClient::apiclient;

NARB_APIClient& NARB_APIClient::instance()
{
    return apiclient;
}

NARB_APIClient::NARB_APIClient()
{
    _host = ""; 
    _port = 0; 
    fd = -1;
}

NARB_APIClient::~NARB_APIClient()
{
}

void NARB_APIClient::setHostPort(const char *host, int port)
{
    _host = host;
    _port = port;
}

int NARB_APIClient::doConnect(char *host, int port)
{
      struct sockaddr_in addr;
      struct hostent *hp;
      int ret;
      int on = 1;

      _host = host;
      _port = port;
  
      assert (strlen(host) > 0 || port > 0);
  	
      hp = gethostbyname (host);
      if (!hp)
      {
          LOG(2)( Log::Routing, "NARB_APIClient::Connect: no such host %s\n", host);
          return (-1);
      }
  
      fd = socket (AF_INET, SOCK_STREAM, 0);
      if (fd < 0)
      {
  	  LOG(2)( Log::Routing, "NARB_APIClient::Connect: socket(): ", strerror (errno));
  	  return (-1);
      }
                                                                                 
      /* Reuse addr and port */
      ret = setsockopt (fd, SOL_SOCKET,  SO_REUSEADDR, (void *) &on, sizeof (on));
      if (ret < 0)
      {
          LOG(1)( Log::Routing, "NARB_APIClient::Connect: SO_REUSEADDR failed.");
          close (fd);
          return (-1);
      }
  
  #ifdef SO_REUSEPORT
    ret = setsockopt (fd, SOL_SOCKET, SO_REUSEPORT,
                      (void *) &on, sizeof (on));
    if (ret < 0)
    {
        LOG(1)( Log::Routing, "NARB_APIClient::Connect: SO_REUSEPORT failed.");
        close (fd);
        return (-1);
    }
  #endif /* SO_REUSEPORT */

    /* Prepare address structure for connect */
    memset (&addr, 0, sizeof (struct sockaddr_in));                                                                                
    memcpy (&addr.sin_addr, hp->h_addr, hp->h_length);
    addr.sin_family = AF_INET;
    addr.sin_port = htons (port);
    //addr.sin_len = sizeof (struct sockaddr_in);
   
    /* Now establish synchronous channel with OSPF daemon */
    ret = connect (fd, (struct sockaddr *) &addr,
                   sizeof (struct sockaddr_in));
    if (ret < 0)
    {
        LOG(2)( Log::Routing, "NARB_APIClient::Connect: connect(): ", strerror (errno));
        close (fd);
        return (-1);
    }

    return fd;
}

int NARB_APIClient::doConnect()
{
    LOG(1)(Log::Routing, "NARB_APIClient connecting ... \n");

    if (_host.length() == 0 || _port == 0)
        return -1;

    if (fd > 0)
        close (fd);
    if (doConnect((char*)(_host.chars()), _port) < 0)
    {
        return -1;
    }

    return fd;
}

void NARB_APIClient::disconnect()
{
    if (fd > 0)
    {
        close (fd);
        fd = -1;
    }
}

bool NARB_APIClient::operational()
{
    if (fd > 0)
        return true;
    if (_host.length() == 0 || _port == 0)
        return false;

    int val, ret=0;
    int sock;
    struct sockaddr_in addr;
    struct hostent *hp;
    int flags, old_flags;
    fd_set sset;
    struct timeval tv;
    socklen_t lon;

    sock = socket (AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
      return -1;

    flags = old_flags = fcntl(sock, F_GETFL, 0);
#if defined(O_NONBLOCK)
    flags |= O_NONBLOCK;
#elif defined(O_NDELAY)
    flags |= O_NDELAY;
#elif defined(FNDELAY)
    flags |= FNDELAY;
#endif

    if (fcntl(sock, F_SETFL, flags) == -1) {
        return false;
    }

    hp = gethostbyname (_host.chars());
    if (!hp)
    {
        LOG(2)(Log::Routing, "NARB_APIClient::Connect: no such host %s\n", _host);
        return (false);
    }

    memset (&addr, 0, sizeof (struct sockaddr_in));
    memcpy (&addr.sin_addr, hp->h_addr, hp->h_length);
    addr.sin_family = AF_INET;
    addr.sin_port = htons (_port);

    ret = connect (sock, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));
    if(ret < 0) {
       if (errno == EINPROGRESS) {
           tv.tv_sec = 1;
           tv.tv_usec = 0;
           FD_ZERO(&sset);
           FD_SET(sock, &sset);
           if(select(sock+1, NULL, &sset, NULL, &tv) > 0) {
               lon = sizeof(int);
               getsockopt(sock, SOL_SOCKET, SO_ERROR, (void*)(&val), &lon); 
               return ((val != 0)? false : true);
           }
           else {
               return false;
           }
       }
       else {
           return false;
       }
   }

    if (fcntl(sock, F_SETFL, old_flags) == -1) {
         return false;
    }
    close (sock);
    return true;
}

EXPLICIT_ROUTE_Object* NARB_APIClient::getExplicitRoute(uint32 src, uint32 dest, uint8 swtype, uint8 encoding, float bandwidth, uint32 vtag)
{
    char buf[1024];
    EXPLICIT_ROUTE_Object* ero = NULL;
    struct narb_api_msg_header* msgheader = (struct narb_api_msg_header*)buf;
    struct msg_app2narb_request* msgbody = (struct msg_app2narb_request*)(buf + sizeof(struct narb_api_msg_header));
    te_tlv_header *tlv = (te_tlv_header*)msgbody;
    int len, offset;
    ipv4_prefix_subobj* subobj_ipv4;
    unum_if_subobj* subobj_unum;

    if (_host.length() > 0 && _port != 0)
        if (doConnect() < 0)
            goto _RETURN;

    //construct NARB API message
    msgheader->type = htons(1); // 1 == NARB_MSG_LSPQ
    msgheader->length = htons (sizeof(struct msg_app2narb_request));
    msgheader->seqnum = htonl (0);
    msgheader->ucid = htonl(0);
    msgheader->tag = htonl(vtag);
    msgheader->options = htonl(0x07<<16); //OPT_STRICT | OPT_PREFERED |OPT_MRN
    if (vtag > 0)
        msgheader->options |= htonl(0x30<<16); //OPT_BIDIRECTIONAL | OPT_E2E_VLAN
    msgheader->chksum = NARB_MSG_CHKSUM(*msgheader);
    memset(msgbody, 0, sizeof(msg_app2narb_request));
    msgbody->type = htons(2); // 2 == MSG_APP_REQUEST
    msgbody->length = htons(sizeof(struct msg_app2narb_request));
    msgbody->src.s_addr = src;
    msgbody->dest.s_addr = dest;
    msgbody->switching_type = swtype;
    msgbody->encoding_type = encoding;
    msgbody->bandwidth = bandwidth;

    //send query
    len = writen(fd, buf, sizeof(struct narb_api_msg_header)+sizeof(struct msg_app2narb_request));
    if (len < 0)
    {
        LOG(2)(Log::Routing, "NARB_APIClient failed to write to: ", fd);
	goto _RETURN;
    }
    else if (len ==0)
    {
	close(fd);
	LOG(1)(Log::Routing, "connection closed for NARB_APIClient.");
        goto _RETURN;
    }
    else if (len != sizeof(struct narb_api_msg_header)+sizeof(struct msg_app2narb_request))
    {
        LOG(2)(Log::Routing, "NARB_APIClient cannot write the message to: ", fd);
        goto _RETURN; 
    } 

    //read reply
    len = readn(fd, buf, sizeof(struct narb_api_msg_header));
    if (len < 0)
    {
        LOG(2)(Log::Routing, "NARB_APIClient failed to read from: ", fd);
	goto _RETURN;
    }
    len = readn(fd, buf+sizeof(struct narb_api_msg_header), ntohs(msgheader->length));
    if (len < 0)
    {
        LOG(2)(Log::Routing, "NARB_APIClient failed to read from: ", fd);
	goto _RETURN;
    }
    //parse NARB reply
    if (ntohs(tlv->type) != 3) // 3 == TLV_TYPE_NARB_ERO
        goto _RETURN;

    ero = new EXPLICIT_ROUTE_Object;
    len = ntohs(tlv->length) ;
    assert( len > 0);
    offset = sizeof(struct te_tlv_header);

    subobj_ipv4  = (ipv4_prefix_subobj *)((char *)tlv + offset);
    while (len > 0)
    {
        if ((subobj_ipv4->l_and_type & 0x7f) == 4)
            subobj_unum = (unum_if_subobj *)((char *)tlv + offset);
        else
            subobj_unum = NULL;

        if (subobj_unum)
        {
            AbstractNode node(((subobj_unum->l_and_type>>7) == 1), NetAddress(subobj_unum->addr.s_addr), subobj_unum->ifid);
      	    ero->pushBack(node);
            len -= sizeof(unum_if_subobj);
            offset += sizeof(unum_if_subobj);
        }
        else
        {
            AbstractNode node(((subobj_ipv4->l_and_type>>7) == 1), NetAddress(*(uint32*)subobj_ipv4->addr), (uint8)32);
      	    ero->pushBack(node);
            len -= sizeof(ipv4_prefix_subobj);
            offset += sizeof(ipv4_prefix_subobj);
        }

        subobj_ipv4  = (ipv4_prefix_subobj *)((char *)tlv + offset);
    }

    if (ero && ero->empty())
    {
        ero->destroy();
        ero = NULL;
    }


_RETURN:
    disconnect();
    return ero;
}

int readn (int fd, char *ptr, int nbytes)
{
  int nleft;
  int nread;

  nleft = nbytes;

  while (nleft > 0) 
    {
      nread = read (fd, ptr, nleft);

      if (nread < 0) 
	return (nread);
      else
	if (nread == 0) 
	  break;

      nleft -= nread;
      ptr += nread;
    }

  return nbytes - nleft;
}  


int writen(int fd, char *ptr, int nbytes)
{
	int nleft;
	int nwritten;

	nleft = nbytes;

	while (nleft > 0) 
	{
	  nwritten = write(fd, ptr, nleft);
	  
	  if (nwritten <= 0) 
         return (nwritten);

	  nleft -= nwritten;
	  ptr += nwritten;
	}
	return nbytes - nleft;
}

