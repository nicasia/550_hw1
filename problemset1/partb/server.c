#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#define _GNU_SOURCE
#include <netdb.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <arpa/inet.h>

#define N_BACKLOG 64

//CONSTANT FOR DEFININIG MAX NO OF CONNECTIONS
#define MAX_CONN 16
//CONSTANT FOR DEFININIG MAX NO OF FDs
#define MAXFDS 64

//CONSTANT FOR DEFININIG BUFFER SIZES
#define CONTENT_SIZE 2000000
#define FILENAME_SIZE 256

// This code has been adapted from https://eli.thegreenplace.net/2017/concurrent-servers-part-3-event-driven/

//DEFINE NECESSARY STRUCTURES FOR THREADS
pthread_t threads[MAX_CONN]={0}; //list of worker threads
pthread_cond_t conditions[MAX_CONN] = {PTHREAD_COND_INITIALIZER}; //condition variables for each thread
pthread_mutex_t locks[MAX_CONN] = {PTHREAD_MUTEX_INITIALIZER}; //lock for each thread
int filename_ptr_ends[MAX_CONN] = {0};
int filecontent_ptr_ends[MAX_CONN] = {0};
char filename_buffers[MAX_CONN][FILENAME_SIZE]={0}; //list of filename buffers for each thread


char filecontent_buffers[MAX_CONN][CONTENT_SIZE] = {0}; //list of file content buffers for each thread
int  thread_ids[MAX_CONN] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15}; //id for each thread
int worker_thread_stats[MAX_CONN] = {0}; //list of keeping track of worker thread availability (0=available, 1=busy)
int thread_pipes[MAX_CONN][2] = {0}; //communication pipes for each thread


//Method for printing which client has connected to the server
void report_peer_connected(const struct sockaddr_in* sa, socklen_t salen) {
  char hostbuf[256];
  char portbuf[256];
  if (getnameinfo((struct sockaddr*)sa, salen, hostbuf, 256, portbuf,256, 0) == 0) {
    printf("peer (%s, %s) connected\n", hostbuf, portbuf);
  } else {
    printf("peer (unknonwn) connected\n");
  }
}

//Method for opening a socket connection
int listen_inet_socket(int portnum, const char* addressnum) {
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("ERROR opening socket");
    exit(EXIT_FAILURE);
   }

  // This helps avoid spurious EADDRINUSE when the previous instance of this
  // server died.
  int opt = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;

  inet_pton(AF_INET, addressnum, &(serv_addr.sin_addr));

  serv_addr.sin_port = htons(portnum);

  if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("ERROR on binding");
    exit(EXIT_FAILURE);
  }

  if (listen(sockfd, N_BACKLOG) < 0) {
    perror("ERROR on listen");
    exit(EXIT_FAILURE);
  }

  return sockfd;
}


//Method for making the scoket connection non-blocking
void make_socket_non_blocking(int sockfd) {
  int flags = fcntl(sockfd, F_GETFL, 0);
  if (flags == -1) {
    perror("fcntl F_GETFL");
    exit(EXIT_FAILURE);
  }

  if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
    perror("fcntl F_SETFL O_NONBLOCK");
    exit(EXIT_FAILURE);
  }
}


//Worker function to read a file and return the contents
void * worker_function(void *args){
    int* arg = (int *) args;

    //Worker thread lives forever
    while(1){
   
      //Wait until the filename is received entirely
      pthread_mutex_lock(&locks[*arg]); 
      pthread_cond_wait(&conditions[*arg], &locks[*arg]); 
      pthread_mutex_unlock(&locks[*arg]); 
  
      int c;
      FILE *fptr;
      char message[CONTENT_SIZE+1];

      int ptr = 0;
      fptr = fopen(filename_buffers[*arg], "r");
      int count;
      if (fptr == NULL){
        strcpy(message, "FILE DOES NOT EXIST");
        count = sizeof("FILE DOES NOT EXIST");
      }
      else{
    	// Read contents from file
        c = fgetc(fptr);
        count = 0;
        while (c != EOF) {
          message[count] = c;
          c = fgetc(fptr);
          count++;
        }
        message[count] = '\0';
        fclose(fptr);  
      }
       
      puts(message);
      write(thread_pipes[*arg][1], message, count); 
    } 
}

//Create struct for keeping track of client state
typedef enum { INITIAL_ACK, WAIT_FOR_MSG, IN_MSG } ProcessingState;

typedef struct {
  ProcessingState state; //processing state for the message, not acknowledged yet/waiting for msg/in message
  int worker_thread_id;
  int filename_read;
} peer_state_t; 

//list of structs for each connection (limited by max fds)
peer_state_t global_state[MAXFDS]; 

// Strucct for monitoring the status of an fd
typedef struct {
  bool want_read;
  bool want_write;
} fd_status_t;

// Constants for states
const fd_status_t fd_status_R = {.want_read = true, .want_write = false};
const fd_status_t fd_status_W = {.want_read = false, .want_write = true};
const fd_status_t fd_status_NAME_W = {.want_read = false, .want_write = false};
const fd_status_t fd_status_RW = {.want_read = true, .want_write = true};
const fd_status_t fd_status_NORW = {.want_read = false, .want_write = false};



//Method to do when a client connects
fd_status_t on_peer_connected(int sockfd, const struct sockaddr_in* peer_addr,socklen_t peer_addr_len) {
  
    assert(sockfd < MAXFDS); //check if max connection limit reached
  	report_peer_connected(peer_addr, peer_addr_len); //just print which client connected

   	//Update peer states
  	peer_state_t* peerstate = &global_state[sockfd];
 	  peerstate->state = WAIT_FOR_MSG;
  	peerstate->filename_read = 0;

  	//FIND AN AVAILABLE THREAD FOR THIS NEW CLIENT
  	for(int i = 0; i < MAX_CONN; i++){
    	if(worker_thread_stats[i] == 0){
          printf("Assigned worker thread %d for client %d \n", i, sockfd);
    			peerstate->worker_thread_id = i;
          worker_thread_stats[i] = 1;
          return fd_status_R; //now we can receive
  		}
  	}
    //No available workexr thread left, we can close the socket
    printf("No available thread exists for client");
    return fd_status_NORW; //we do not want to receive or send anymore
}

//Method to do when server wants to receive from client
fd_status_t on_peer_ready_recv(int sockfd, struct epoll_event *event) {
  
	assert(sockfd < MAXFDS); //check if max connection limit reached
  
	peer_state_t* peerstate = &global_state[sockfd];

  	if (peerstate->state == INITIAL_ACK) {
	    return fd_status_R;
  	}

  	//NOW, START RECEIVING 
  	char buf[FILENAME_SIZE];
  	int nbytes = recv(sockfd, buf, sizeof buf, 0);
  	
    if (nbytes == 0) {
    		// The peer disconnected.
    		return fd_status_NORW;
  	} else if (nbytes < 0) {
	    	if (errno == EAGAIN || errno == EWOULDBLOCK) {
	     	 	// The socket is not *really* ready for recv; wait until it is.
	      	return fd_status_R;
	    	} else {
	      	perror("recv");
	      	exit(EXIT_FAILURE);
	    	}
	}
 
  	//KEEP RECEIVING THE MESSAGE FROM CLIENT
  	bool ready_to_send = false;
  	
	for (int i = 0; i < nbytes; ++i) {
    		switch (peerstate->state) {
		      case INITIAL_ACK:
		        assert(0 && "can't reach here");
		        break;
		      case WAIT_FOR_MSG:
		        if ((buf[i]) != '\n') { //message did not start yet, wait for a char
              peerstate->state = IN_MSG;
		        }
		        //break;
    		  case IN_MSG:
            if (buf[i] == '\n') { //message has ended
					int index = peerstate->worker_thread_id;

					  filename_buffers[index][filename_ptr_ends[index]] = '\0';
					  // filename_ptr_ends[index] = filename_ptr_ends[index] + 1;
  				    ready_to_send = true; //Message ended, now we can send
				      //Signal the worker function that filename is complete, execution can start  					
				      pthread_mutex_lock(&locks[peerstate->worker_thread_id]); 
				      pthread_cond_signal(&conditions[peerstate->worker_thread_id]); 
				      pthread_mutex_unlock(&locks[peerstate->worker_thread_id]); 
				      peerstate->state = WAIT_FOR_MSG;
				      return fd_status_W;
            } else { 
				      int index = peerstate->worker_thread_id;
				      filename_buffers[index][filename_ptr_ends[index]] = buf[i];
				      filename_ptr_ends[index] = filename_ptr_ends[index] + 1;
      		  }
      		  break;
    		}
	}

  // Now we are ready to send to client
  return (fd_status_t){.want_read = !ready_to_send,
                       .want_write = ready_to_send};
}


//Method to do when server wants to send to client
fd_status_t on_peer_ready_send(int sockfd) {

  	assert(sockfd <  MAXFDS);  //check if max connection limit reached

  	peer_state_t* peerstate = &global_state[sockfd];

  	//if filename is not read yet, read it from pipe
  	if(peerstate-> filename_read == 0){
  
		char buf[CONTENT_SIZE];

    int byte = read(thread_pipes[peerstate->worker_thread_id][0], buf, sizeof buf);
    
    		if(byte > 0){
      			for(int i = 0; i < byte; i++){
              if (buf[i] == '\0') {
				filecontent_buffers[peerstate->worker_thread_id][filecontent_ptr_ends[peerstate->worker_thread_id]] ='\0';
                peerstate-> filename_read = 1;
                return fd_status_W; //NOW, we have the filename, we can send the msg
              } else {
                filecontent_buffers[peerstate->worker_thread_id][filecontent_ptr_ends[peerstate->worker_thread_id]] = buf[i];
                filecontent_ptr_ends[peerstate->worker_thread_id]++;
              }
        			if(i == byte - 1){
                peerstate-> filename_read = 1;
       					return fd_status_W;
                //break; //NOW, we have the filename, we can send the msg
    				  }
      			}    
    		} else{
            return fd_status_W;
        }
    		//peerstate-> filename_read = 1; //update filename read status
  	} else {
  		  int sendlen = sizeof filecontent_buffers[peerstate->worker_thread_id];
    		int nsent = send(sockfd, filecontent_buffers[peerstate->worker_thread_id], sendlen, 0);
	    	if (nsent == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
		    		    return fd_status_W;
            } else {
		    		    perror("send");
		    		    exit(EXIT_FAILURE);
            }
	    	}
    
  	if (nsent < sendlen) {
        return fd_status_W;
    } else {
        // Everything was sent successfully; reset the send queue.
        
        // Special-case state transition in if we were in INITIAL_ACK until now.
        if (peerstate->state == INITIAL_ACK) {
            peerstate->state = WAIT_FOR_MSG;
        }

    		//Empty the thread now, this thread can be available again
   			peerstate-> filename_read = 0;
        worker_thread_stats[peerstate->worker_thread_id] = 0; 
        filename_ptr_ends[peerstate->worker_thread_id] = 0;
        filecontent_ptr_ends[peerstate->worker_thread_id] = 0;
        memset(filename_buffers[peerstate->worker_thread_id] , 0, sizeof filename_buffers[peerstate->worker_thread_id] );
        memset(filecontent_buffers[peerstate->worker_thread_id] , 0, sizeof filecontent_buffers[peerstate->worker_thread_id] );	
      	
		filecontent_buffers[peerstate->worker_thread_id][0] = '\0'
		filename_buffers[peerstate->worker_thread_id][0] = '\0';
		return fd_status_NORW;
    }
  }
}




//MAIN LOOP OF EXECUTION
int main(int argc, const char** argv) {

  setvbuf(stdout, NULL, _IONBF, 0);

  const char* addressnum = "127.0.0.1"; //default address
  int portnum = 9000; //default port
  
  if (argc >= 3) {
    addressnum = argv[1];
    portnum = atoi(argv[2]);
  }
  printf("Serving at %s on port %d\n", addressnum, portnum);

  //Create socket for connection and set to non-blocking mode
  int listener_sockfd = listen_inet_socket(portnum, addressnum);
  make_socket_non_blocking(listener_sockfd);

  //Create epoll for managing multiple connections
  int epollfd = epoll_create1(0);
  if (epollfd < 0) {
    perror("epoll_create1");
    exit(EXIT_FAILURE);
  }

  //A struct for epoll event which has fd and status
  struct epoll_event accept_event;

  accept_event.data.fd = listener_sockfd;
  accept_event.events = EPOLLIN;
  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listener_sockfd, &accept_event) < 0) {
    perror("epoll_ctl EPOLL_CTL_ADD");
    exit(EXIT_FAILURE);
  }

  //Create eooll events for each of the clients + main up to max capacity
  struct epoll_event* events = calloc(MAXFDS, sizeof(struct epoll_event));
  if (events == NULL) {
    perror("Unable to allocate memory for epoll_events");
    exit(EXIT_FAILURE);
  }

  //CREATE A THREADPOOL
  for(int th = 0; th < MAX_CONN; th++){
       pipe(thread_pipes[th]);
       worker_thread_stats [th] = 0;
       int thread_create = pthread_create(&threads[th], NULL, worker_function, &thread_ids[th]);
       
       make_socket_non_blocking(thread_pipes[th][0]);
       
       struct epoll_event ev;
       ev.data.fd = thread_pipes[th][0];
       ev.events = EPOLLIN;
       epoll_ctl( epollfd, EPOLL_CTL_ADD, thread_pipes[th][0], &ev);
   }
  


  //Continue executing the server forever
  while (1) {
    
    //Wait for sockets that are ready
    int nready = epoll_wait(epollfd, events, MAXFDS, -1);
    
    //Loop through all ready sockets
    for (int i = 0; i < nready; i++) {
      if (events[i].events & EPOLLERR) {
        perror("epoll_wait returned EPOLLERR");
        exit(EXIT_FAILURE);
      }

      if (events[i].data.fd == listener_sockfd) {
        // The listening socket is ready; this means a new peer is connecting.

        struct sockaddr_in peer_addr;
        socklen_t peer_addr_len = sizeof(peer_addr);
        int newsockfd = accept(listener_sockfd, (struct sockaddr*)&peer_addr,
                               &peer_addr_len);
        if (newsockfd < 0) {
          if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // This can happen due to the nonblocking socket mode; in this
            // case don't do anything, but print a notice (since these events
            // are extremely rare and interesting to observe...)
            printf("accept returned EAGAIN or EWOULDBLOCK\n");
          } else {
            perror("accept");
            exit(EXIT_FAILURE);
          }
        } else {


          make_socket_non_blocking(newsockfd);

          if (newsockfd >= MAXFDS) {
            printf("socket fd (%d) >= MAXFDS (%d)", newsockfd, MAXFDS);
            exit(EXIT_FAILURE);
          }

                       
          fd_status_t status = on_peer_connected(newsockfd, &peer_addr, peer_addr_len);

          struct epoll_event event = {0};
          event.data.fd = newsockfd;
          
          if (status.want_read) {
            event.events |= EPOLLIN;
          }
          if (status.want_write) {
            event.events |= EPOLLOUT;
          }
          if (status.want_write == 0 && status.want_read == 0) {
            close(newsockfd);
          } else {
            if (epoll_ctl(epollfd, EPOLL_CTL_ADD, newsockfd, &event) < 0) {
              perror("epoll_ctl EPOLL_CTL_ADD");
              exit(EXIT_FAILURE);
            }
          }  
        }
      } else {
        // A peer socket is ready.

        if (events[i].events & EPOLLIN) {
          // Ready for reading.
          int fd = events[i].data.fd;
          fd_status_t status = on_peer_ready_recv(fd, &events[i]);
          struct epoll_event event = {0};
          event.data.fd = fd;
          if (status.want_read) {
            event.events |= EPOLLIN;
          }
          if (status.want_write) {
            event.events |= EPOLLOUT;
          }
          if (event.events == 0) {
            if (epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL) < 0) {
              perror("epoll_ctl EPOLL_CTL_DEL");
              exit(EXIT_FAILURE);
            }
            close(fd);
          } else if (epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event) < 0) {
            perror("epoll_ctl EPOLL_CTL_MOD");
            exit(EXIT_FAILURE);
          }
        } else if (events[i].events & EPOLLOUT) {
          // Ready for writing.
          int fd = events[i].data.fd;
          fd_status_t status = on_peer_ready_send(fd);
          struct epoll_event event = {0};
          event.data.fd = fd;

          if (status.want_read) {
            event.events |= EPOLLIN;
          }
          if (status.want_write) {
            event.events |= EPOLLOUT;
          }
          if (event.events == 0) {
            printf("socket %d closing\n", fd);
            if (epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL) < 0) {
              perror("epoll_ctl EPOLL_CTL_DEL");
              exit(EXIT_FAILURE);
            }
            int close_stat = close(fd);
          } else if (epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event) < 0) {
            perror("epoll_ctl EPOLL_CTL_MOD");
            exit(EXIT_FAILURE);
          }
        }
      }
    }
  }
  return 0;
}
