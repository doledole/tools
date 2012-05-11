#include "telnet_server.h"
char host[MAX_HOST];//server ip address
int port;//server port
FILE *logfile;//logfile pointer
pthread_mutex_t log_lock;//log file lock
pthread_mutex_t var_lock;//session lock
char log_file_name[MAX_DIR];//log file path
char work_dir[MAX_DIR];//server work directory
struct session *session_head;//session  link head
int keepalive;//1 if keep the socket alive
int master_socket;//service socket
int nclients;//clients number
telnet_mode_t max_telnet_mode; //available NO_LINEMODE,KLUDGE_LINEMODE,REAL_LINEMODE
//initiate  server
void init_server_cfg()
{
    init_fsm();//��ʼ��״̬��
    pthread_mutex_init(&log_lock,0);//��ʼ����
    pthread_mutex_init(&var_lock,0);
#ifdef DEFAULTIP//���÷�����ip��ַ
    memcpy(host,DEFAULTIP,strlen(DEFAULTIP)+1);
#else
    memcpy(host,"localhost",strlen("localhost")+1);
#endif
#ifdef DEFAULTPORT//���÷�����Ĭ�϶˿�
    port=DEFAULTPORT;
#else
    port=23;
#endif
#ifdef DEFAULTIDR//���÷���������Ŀ¼
	memcpy(work_dir,DEFAULTDIR,strlen(DEFAULTDIR)+1);
#else
    memcpy(work_dir,"./",strlen("./"));
#endif
#ifdef DEFAULTLOG//������־�ļ�
	memcpy(log_file_name,DEFAULTLOG,strlen(DEFAULTLOG)+1);
#else
    memcpy(log_file_name,"./telnet-server.log",strlen("./telnet-server.log")+1);
#endif	
	keepalive=0;
#ifdef MAX_TELNET_MODE
	max_telnet_mode=MAX_TELNET_MODE;//���÷��������֧��ģʽ
#else
    max_telnet_mode=NO_LINEMODE;
#endif
	session_head=0;//session����ʼ��Ϊ��
	logfile= fopen(log_file_name, "a+");//����־�ļ�
    if (!logfile)
        perror("fopen()");
	wrtlog("host=%s  port=%d work_directory=%s log_file=%s(pid: %d)\n",
	      host,port,work_dir,log_file_name,getpid());
}
//begin service
int telnet_service(char *ip, int port_number)
{
	if(ip != NULL) {
		strcpy(host, ip);
	}

/*	int port_n = -1;
	if(port_number == 0)
		port_n = port;
	else 
		port_n = port_number;*/

	int port_n = port_number;
   struct sockaddr_in addr;
   int addrlen,on=1;
   chdir(work_dir);//�л�������Ŀ¼
   signal(SIGCHLD, SIG_IGN);
   if ((master_socket= socket(PF_INET, SOCK_STREAM, 0)) < 0) {//�������׽���
        perror("socket()");
	    return 1;
    }
    setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, &on,sizeof(on));//������ַ����
	setsockopt(master_socket,SOL_SOCKET,SO_OOBINLINE,(char*)&on,sizeof(on));
	if (keepalive
      && setsockopt (master_socket, SOL_SOCKET, SO_KEEPALIVE,(char *) &on, sizeof (on)) < 0)
	{
	    perror("keepalive");
	}
	memset(&addr,0,sizeof(addr));
	addr.sin_family = AF_INET;
    addr.sin_port = htons(port_n);
    if((addr.sin_addr.s_addr = inet_addr(host))==INADDR_NONE)//���÷�������ַ
	    addr.sin_addr.s_addr=INADDR_ANY;
    addrlen = sizeof(struct sockaddr_in);
    if (bind(master_socket, (struct sockaddr *) &addr, addrlen) < 0) {
		close(master_socket);
		perror("bind()");
		return 1;
    }
#ifdef MAX_CLINETS_WAIT
	if (listen(master_socket, MAX_CLINETS_WAIT) < 0) {
#else
    if(listen(master_socket,5)<0){
#endif
		perror("listen()");
		return 1;
    }
	while (1) {
	    int sock;
		struct session *sp=0;
        addrlen = sizeof(struct sockaddr_in);
		//wrtlog("in service loop:\n\r");
        sock= accept(master_socket, (struct sockaddr *) &addr, &addrlen);
        if (sock < 0) {
            perror("accept()");
			clean_all();//fatal error
			return;
        }
		if(!(sp=create_session()))//����һ��session
		{
		    wrtlog("can't create session\n");
			close(sock);
			continue;
		}
		sp->client_socket=sock;//�����׽���
		time(&sp->log_time);		//��¼��¼ʱ��
		set_client_addr(sp,&addr);//��¼�ͻ��˵�ַ
		//struct linger lgr;
	    //lgr.l_onoff=1;
	    //lgr.l_linger=0;
	    //setsockopt(sp->client_socket,SOL_SOCKET,SO_LINGER,&lgr,sizeof(struct linger));
        wrtlog("Request from: %s at %s\n",sp->client_addr,ctime(&sp->log_time));
		if(pthread_create(&(sp->thread_id),NULL,(void* (*)(void*))do_service,(void*)sp))//�����߳̿�ʼ����
		{
		    wrtlog("error create thread\n");
		    wrtlog(strerror(errno));
		    clean_all();
		    return;
		}		
        if(pthread_detach(sp->thread_id)!=0)
		{
		    wrtlog(strerror(errno));
		    clean_all();
		    return;
		}
	}
}
//serve a client
int do_service(struct session* sp)
{
	fd_set in,out,exp;
#ifdef MAX_INACTIVE_TIME_IN_SEC//���÷��������ȴ��ͻ��˵�ʱ��
	struct timeval time_span={MAX_INACTIVE_TIME_IN_SEC,0};
#else
    struct timeval time_span={60*5,0};
#endif
	sp->logoff=0;//���ÿͻ����Ƿ��˳���־
	time(&sp->log_time);//��¼�ͻ���¼ʱ��
	dup2(sp->client_socket, 0);//���Ʊ�׼����
    //dup2(sp->client_socket, 1);
    //dup2(sp->client_socket, 2);
	init_term(sp);//��ʼ���ն���Ϣ
	get_slc_defaults (sp);//����slc
	io_setup(sp);//��ʼ��������ָ��
	sp->telnet_mode=NO_LINEMODE;//����Ĭ��ģʽ���ַ�ģʽ
	sp->max_telnet_mode=max_telnet_mode;//���ÿͻ��˵����֧��ģʽ
	if (my_state_is_wont (sp,TELOPT_SGA))//����������sga
        send_will (sp,TELOPT_SGA, 1);
    send_do (sp,TELOPT_ECHO, 1);//���Կͻ����Ƿ��ǽ��ϵ�ϵͳ
    if (his_state_is_wont (sp,TELOPT_LINEMODE)&&sp->max_telnet_mode>=REAL_LINEMODE)//���֧����ģʽ������������ģʽ
    {
      /* Query the peer for linemode support by trying to negotiate
         the linemode option. */
	  //sp->telnet_mode=NO_LINEMODE;
        sp->linemode = 0;
        sp->editmode = 0;
        send_do (sp,TELOPT_LINEMODE, 1);	/* send do linemode */
    }
	send_do (sp,TELOPT_TTYPE,1);
    send_do (sp,TELOPT_NAWS, 1);//��ͼ��ȡ�ͻ��˵Ĵ��ڴ�С
	while(his_will_wont_is_changing (sp,TELOPT_NAWS)&&!sp->logoff)
	{
	    io_drain(sp);
	}
	if (his_want_state_is_will (sp,TELOPT_ECHO) && his_state_is_will (sp,TELOPT_NAWS))//����echoѡ��
	{
        while(his_will_wont_is_changing (sp,TELOPT_ECHO)&&!sp->logoff)
	    {
	        io_drain(sp);
	    }
	}
	if (sp->telnet_mode < REAL_LINEMODE&&sp->max_telnet_mode>=KLUDGE_LINEMODE)//����ͻ��˾ܾ�ʵ��ģʽ������Э��׼��ģʽ
	{
        send_do (sp,TELOPT_TM, 1);
		//io_drain(sp);
	}
	/*
	if (his_want_state_is_will (TELOPT_ECHO))
    {
        w_echo(TELOPT_ECHO);
    }*/
	if (my_state_is_wont (sp,TELOPT_ECHO)&&sp->telnet_mode==NO_LINEMODE)//�ͻ��˲�֧����ģʽ������Ĭ��ģʽ
	{
        send_will (sp,TELOPT_ECHO, 1);
		//io_drain(sp);
	}
	if(!sp->logoff)
	{
	    io_drain(sp);
	    localstat (sp);//���ݷ���״̬����ѡ��Э��
	    if(my_state_is_wont(sp,TELOPT_ECHO)&&sp->telnet_mode==NO_LINEMODE)//����windows�ͻ��ˣ���ʱ���ַ�ģʽ��ȴҪ�����˲��ṩ���ԣ���ʱǿ�����÷���˻��ԣ���������ͻ��˷��͸�ѡ��
	    {
	        set_my_state_will(sp,TELOPT_ECHO);
	        set_my_want_state_will(sp,TELOPT_ECHO);
	    }
	}
	//client_wrtlog(sp,"From %s begin loop\n",sp->client_addr);
	while(!sp->logoff)
	{
	   int s;
	   FD_ZERO(&in);//�����������
	   FD_ZERO(&out);
	   FD_ZERO(&exp);
	   if(!net_in_full(sp))//�������뻺��δ�����ɶ��׽���
	   {
	        FD_SET(sp->client_socket,&in);
	   }
	   if(net_out_rem(sp)>0)//�����������δ�գ���д�׽���
	   {
	        FD_SET(sp->client_socket,&out);
	   }
	   if(!sp->syn_num)//δ���յ������źţ��ɽ��մ�������
	   {
	        FD_SET(sp->client_socket,&exp);
	    }
		if ((s = select (sp->client_socket+1, &in, &out, &exp, &time_span)) <= 0)
	    {
		    //perror("select()");
	        if (s == -1 && errno == EINTR)//���ź��ж�
	            continue;
			else//��ʱ���ر��׽���
			{
			   //return 0;
			   //sleep(5);
			    cwrtlog(sp,"time out.going to logout\n\r");
				net_put(sp,"Inactive too long...Please log again\n\r");
			    logout(sp);
			    break;
			}
	    }
		if (FD_ISSET (sp->client_socket, &exp))//������յ������źţ���ͬ���ź�
		{
	        sp->syn_num = 1;
			wrtlog("From %s get a syn\n\r",sp->client_addr);
		}
        if (FD_ISSET (sp->client_socket, &in))//�׽��ֿɶ�����ȡ�׽���
	    {
	    /* Something to read from the network... */
	    /*FIXME: handle  !defined(SO_OOBINLINE) */
	        if(net_read (sp)<=0)//�׽��ֹرգ�����session���˳�
		    {
				cmd_out_flush(sp);
				net_flush(sp);
				time_t logofftime;
				time(&logofftime);
				cwrtlog(sp,"logoff at %s\n",ctime(&logofftime));
	            clean(sp);
		        pthread_exit(NULL);
				return;
		    }
	    }
		if(sp->cmd_outend>sp->cmd_outbeg&&!net_out_full(sp))//�����������buffer���գ����
		{
		    cmd_out_flush(sp);
		}
		if (FD_ISSET (sp->client_socket, &out) &&sp->net_outend-sp->net_outbeg > 0)//�׽��ֿ������������绺��
	        net_flush (sp);
        if (sp->net_inlen> sp->net_inp-sp->net_inbuffer)//����������л�õ�����
		{
	        dispose(sp);
		}
		//set_line_mode(REAL_LINEMODE);
		
	}
	clean(sp);//����session���˳�
	pthread_exit(NULL);
}
struct session* create_session()
{
    pthread_mutex_lock(&var_lock);
    if(nclients>=MAX_CLIENTS)//����ͬʱ����Ŀͻ���Ŀ
    {
	    wrtlog("too much clients\n");
		pthread_mutex_unlock(&var_lock);
		return NULL;
    }
    if(!session_head)//������һ��session
    {
	    session_head=malloc(sizeof(struct session));
	    if(!session_head)
	    {
		    pthread_mutex_unlock(&var_lock);
		    return NULL;
	    }
	    memset(session_head,0,sizeof(struct session));
    }
    else//����session
    {
	    struct session *sp;
	    sp=malloc(sizeof(struct session));
	    if(!sp)
	    {
		    pthread_mutex_unlock(&var_lock);
		    return NULL;
	    }
        memset(sp,0,sizeof(struct session));
	    sp->next=session_head;
        session_head=sp;
    }
    nclients++;
    pthread_mutex_unlock(&var_lock);
    return session_head;
}
