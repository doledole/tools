#include "telnet_server.h"
//extern pthread_mutex_t log_lock;//��־�ļ���
//extern pthread_mutex_t var_lock;//session��
//extern int nclients;//�ͻ���Ŀ
//extern FILE* logfile;//��־�ļ�ָ��
//extern unsigned char trans_table[][NCHARS];//��״̬ת�ƾ���
//extern unsigned char subtrans_table[][NCHARS];//��ѡ��״̬ת�ƾ���
//extern struct fsm trans[];//��״̬����
//extern struct fsm subtrans[];//��ѡ��״̬����
//extern struct session *session_head;//sessionͷ
//extern char crlf[];//���з�
//extern int master_socket;//���׽���
//��ʼ������ָ��
void io_setup (struct session* sp)
{
    sp->cmd_outbeg = sp->cmd_outend=sp->cmd_outbuffer;
    sp->net_outbeg=sp->net_outend=sp->net_outbuffer;
    sp->net_inp = sp->net_inbuffer;
    sp->cmd_inp = sp->cmd_inbuffer;
    sp->net_inlen=0;
    sp->cmd_inlen=0;
}
/*
���������뻺����ȡlen���ַ�������bָ��Ļ����У����forward����0������ָ�����ǰ�ƶ�������ָ�벻�ƶ�
����ʵ�ʶ�ȡ���ַ���
*/
int net_get(struct session *sp,char *b,int len,int forward)
{
    int nlen=sp->net_inbuffer+sp->net_inlen-sp->net_inp;
    if(nlen<len)
        len=nlen;
    if(b!=NULL)
        memcpy(b,sp->net_inp,len);
    if(forward)
    {
        sp->net_inp+=len;
    }
    if(sp->net_inp>=sp->net_inbuffer+sp->net_inlen)//��������Ѷ��꣬���������
    {
        sp->net_inp=sp->net_inbuffer;
	    sp->net_inlen=0;
    }
    return len;
}
/*
�����ݷ���������������У�����Ϊ�ɱ����
    sp:��Ҫ�����session
	format:Ҫ����ĸ�ʽ���ַ���
	����ʵ��������ַ���
*/
int net_put(struct session *sp,const char *format,...)
{
   //wrtlog("in net_put\n");
    va_list args;
    int len;
    len=strlen(format);
    int rem=sp->net_outbuffer+sizeof(sp->net_outbuffer)-sp->net_outend;
    while(rem<len)
    {
        net_flush(sp);
	    rem=sp->net_outbuffer+sizeof(sp->net_outbuffer)-sp->net_outend;
    }
    va_start(args,format);
    len=vsnprintf(sp->net_outend,rem,format,args);
    va_end(args);
   //wrtlog("in net_put:buf=%s\n",sp->net_outend);
    if(len>rem)
        len=rem-1;
    sp->net_outend+=len;
    return len;
}
/*
   ������Ϊlen���ַ���src����sp���������������
   ����ʵ��������ַ���
*/
int net_put_len(struct session *sp,char *src,int len)
{
   //wrtlog("in net_put_len\n\r");
   int rem=sp->net_outbuffer+MAX_BUF_OUT-sp->net_outend;//�������������岻����ˢ�»�����
    while(rem<len)
    {
        net_flush(sp);
	    rem=sp->net_outbuffer+MAX_BUF_OUT-sp->net_outend;
    }
   
   //if(sp->net_outbuffer+MAX_BUF_OUT-sp->net_outend<len)
   //   len=sp->net_outbuffer+MAX_BUF_OUT-sp->net_outend;
    memcpy(sp->net_outend,src,len);
    sp->net_outend+=len;
    return len;
}
/*
int net_put_len_ln(char *src,int len)
{
   return net_put_len(src,len)+net_put("\r\n");
}*/
/*
    ˢ�����绺��
	sp:Ҫˢ�µ�session
*/
void net_flush(struct session* sp)
{
    int len=sp->net_outend-sp->net_outbeg;
  // wrtlog("in net_flush len=%d buff=%s\n",len,sp->net_outend);
    if(len>0)
    {
        if(sp->urgent!=0)
	    {
		    len=send(sp->client_socket,sp->net_outbeg,sp->urgent-sp->net_outbeg,MSG_OOB);
	    }
	    else
	        len=send(sp->client_socket,sp->net_outbeg,len,0);
    }
    sp->net_outbeg+=len;
    if(sp->net_outbeg==sp->net_outend)
    {
        sp->net_outbeg=sp->net_outend=sp->net_outbuffer;
    }
    if(sp->net_outbeg>=sp->urgent)
        sp->urgent=0;
}
/*
  �����������Ҫ���ý������ݣ��ڵ���net_put����net_put_len���ͽ������ݺ�
  ����set_neturg()���ý���ָ��
*/
void set_neturg (struct session* sp)
{
    sp->urgent = sp->net_outend - 1;
}
/*
   �������ж�ȡ����
   ���ض�ȡ���ַ���
*/
int net_read (struct session* sp)
{
    sp->net_inlen = read (sp->client_socket, sp->net_inbuffer, MAX_BUF_IN);
    if (sp->net_inlen < 0 && errno == EWOULDBLOCK)
        sp->net_inlen = 0;
  //else if (sp->net_inlen == 0)
  //{
  //    wrtlog("From %s telnetd:  peer died\n",sp->client_addr);
  //}
    sp->net_inp= sp->net_inbuffer;
 // wrtlog("From %s get %d bytes\n",sp->client_addr,sp->net_inlen);
     return sp->net_inlen;
}
int net_out_full(struct session* sp)//����������������Ƿ�����
{
    return sp->net_outend>=sp->net_outbuffer+sizeof(sp->net_outbuffer);
}
int net_out_rem(struct session* sp)//���������������ʣ��Ŀռ�
{
    return sp->net_outend-sp->net_outbeg;
}
int net_in_full(struct session* sp)//�����������뻺���Ƿ�����
{
    return sp->net_inlen>=sizeof(sp->net_inbuffer);
}
/*
   ��������ݷ����������������buffer��
   sp:��Ӧ��session�ṹ
   data:Ҫ���������
   len:Ҫ����ĳ���
   ���������Ȳ��ܳ���MAX_BUF_OUT
*/
void cmd_output (struct session *sp,const char *data, int len)
{
    //need filter slc
    //printf("From %s in cmd_output input=%s len=%d strlen(data)=%d\n",sp->client_addr,(char*)data,len,strlen((char*)data));
    if ((&sp->cmd_outbuffer[MAX_BUF_OUT] - sp->cmd_outend) <len)
    {
        cmd_out_flush (sp);
    }
    if(len>&sp->cmd_outbuffer[MAX_BUF_OUT]-sp->cmd_outend)
        len=&sp->cmd_outbuffer[MAX_BUF_OUT]-sp->cmd_outend;
    memcpy (sp->cmd_outend, data, len);
    sp->cmd_outend += len;
}
/*
  ͬcmd_output()
  �����������ϻ��з�
*/
void cmd_output_ln(struct session *sp,char *data,int len)
{
    cmd_output(sp,data,len);
    cmd_output(sp,crlf,strlen(crlf));
}
/*
   ����������buffer�з�����������
   src:ָ��Ҫ���������
   len:���ݳ���
   ����ʵ�ʷ���ĳ���
*/
int cmd_input_len(struct session* sp,char *src,int len)
{
    if(MAX_BUF_IN-sp->cmd_inlen<len)
	    len=MAX_BUF_IN-len;
	memcpy(sp->cmd_inbuffer+sp->cmd_inlen,src,len);
	sp->cmd_inlen+=len;
	return len;
}
/*
   ˢ�������������
   ��ɵĹ�����
   1����IAC(255)���ַ�ǰ�����һ��IAC
   2�����ڲ�֧�ְ�λ�ַ��Ŀͻ��ˣ�����ַ��ĵڰ�λ
   3����������ֻ����һ��'\r'���Զ���������'\n'��0
   4����������������
   �ڵ���cmd_output������ô˺���������ˢ�µ��������������
*/
void cmd_out_flush (struct session* sp)
{
    int n;
    // wrtlog("From %s in cmd_out_flush:len=%d \n",sp->client_addr,sp->cmd_outend - sp->cmd_outbeg);
    char *p=sp->cmd_outbeg;
    if ((n = sp->cmd_outend - sp->cmd_outbeg) > 0)
    {  
        for(p=sp->cmd_outbeg;p<sp->cmd_outend;p++)
	    {
	        if(*p==(char)IAC)
		        net_put_len(sp,p,1);
		    net_put_len(sp,p,1);
		    if(my_state_is_wont(sp,TELOPT_BINARY))
		        *p=*p&0x7f;
		    if(*p=='\r'&&my_state_is_wont (sp,TELOPT_BINARY))
		    {
		        if (p<=sp->cmd_outend-2 && *(p+1) == '\n')
			    {
		            net_put_len(sp,++p,1);
			    }
	            else
			    {
			        char c='\0';
		            net_put_len(sp,&c,1);
			    }
            }
	    }
    }
    sp->cmd_outbeg=sp->cmd_outend=sp->cmd_outbuffer;
}
//���������뻺���л�ȡlen���ַ����洢��b�У����forward����0������ǰ�ƶ�ָ��
int cmd_get(struct session *sp,char *b,int len,int forward)
{
    int nlen=sp->cmd_inlen+sp->cmd_inbuffer-sp->cmd_inp;
    if(nlen<len)
        len=nlen;
    memcpy(b,sp->cmd_inp,len);
    if (forward)
        sp->cmd_inp+=len;
    if(sp->cmd_inp>=sp->cmd_inbuffer+sp->cmd_inlen)
    {
        sp->cmd_inp=sp->cmd_inbuffer;
	    sp->cmd_inlen=0;
    }
    return len;
}
/*
int cmd_input_putback (const char *str, size_t len)
{
  if (len > &sp->cmd_inbuffer[MAX_BUF-1] - sp->cmd_inp)
    len = &sp->cmd_inbuffer[MAX_BUF-1] - sp->cmd_inp;
  strncpy (sp->cmd_inp, str, len);
  sp->cmd_inlen+=len;
}
*/
/*
int cmd_read ()
{
  sp->cmd_inlen = read (cmd, sp->cmd_inbuffer, MAX_BUF);
  if (sp->cmd_inlen < 0 && (errno == EWOULDBLOCK
#ifdef	EAGAIN
		  || errno == EAGAIN
#endif
		  || errno == EIO))
    sp->cmd_inlen = 0;
  sp-> cmd_inp= sp->cmd_inbuffer;
  return sp->cmd_inp;
}
*/
int cmd_out_full(struct session* sp)//����������������Ƿ�����
{
    return (sp->cmd_outend>=sp->cmd_outbuffer+sizeof(sp->cmd_outbuffer)-1);
}
int cmd_in_rem(struct session* sp)//�����������뻺��ʣ��ռ�
{
    return (sizeof(sp->cmd_inbuffer)-sp->cmd_inlen-1);
}
/*
  �����绺���ж�ȡ�ַ�����Ȼ������ַ���
*/
void io_drain (struct session* sp)
{
  //wrtlog("From %s in io_drain\r\n",sp->client_addr);
    if (sp->net_outend - sp->net_outbeg > 0)
        net_flush (sp);
again:
    sp->net_inlen = read (sp->client_socket, sp->net_inbuffer, sizeof(sp->net_inbuffer));
    if (sp->net_inlen < 0)
    {
        if (errno == EAGAIN)
	    {
	        goto again;
	    }
        //clean(sp);
	    sp->logoff=1;
	    return;
    }
    else if (sp->net_inlen == 0)
    {
        clean(sp);
    }
    sp->net_inp = sp->net_inbuffer;
    dispose(sp);		/* state machine */
    if (sp->net_inp <sp->net_inbuffer+sp->net_inlen)//���������������������ˢ�¸û��壬����ʣ�µ��ַ�
    {
        cmd_out_flush(sp);
        dispose(sp);
    }
}
char *nextitem (char *current)
{
    if ((*current & 0xff) != IAC)
        return current + 1;
    switch (*(current + 1) & 0xff)
    {
        case DO:
        case DONT:
        case WILL:
        case WONT:
            return current + 3;
        case SB:			/* loop forever looking for the SE */
        {
	        char *look = current + 2;
	        for (;;)
	        if ((*look++ & 0xff) == IAC && (*look++ & 0xff) == SE)
	             return look;
        }
        default:
	        return current + 2;
    }
}
/*
  ���һ������Ϊlen�Ļ����е����ݣ�����������
*/
#define wewant(p) \
  ((buf_beg+len > p) && ((*p&0xff) == IAC) && \
   ((*(p+1)&0xff) != EC) && ((*(p+1)&0xff) != EL))
int buffer_clear (char *buf_beg,char *cmd_start,int len)//clear all data but cmd 
{
    char *thisitem, *next;
    char *good;
    thisitem = buf_beg;
    while ((next = nextitem (thisitem)) <= cmd_start)
        thisitem = next;

    /* Now, thisitem is first before/at boundary. */
    good = buf_beg;		/* where the good bytes go */
    while (buf_beg+len> thisitem)
    {
        if (wewant (thisitem))
	    {
	        int length;
	        for (next = thisitem; wewant (next) && buf_beg+len > next;next = nextitem (next));
	        length = next - thisitem;
	        memmove (good, thisitem, length);
	        good += length;
	        thisitem = next;
	    }
        else
	    {
	        thisitem = nextitem (thisitem);
	    }
    }
    return good-buf_beg;
  //sp->net_outbeg = sp->net_outbuffer;
  //sp->net_outbeg = good;		/* next byte to be sent */
  //sp->urgent = 0;
}				/* end of netclear */
/*
  ����־�ļ�д����־��
  ����Ϊ�ɱ����
*/					
void wrtlog(char *format,...)
{
	va_list arg;
#ifdef _TELNET_DEBUG_
	char buff[MAX_BUF_OUT];
#endif
    if(logfile)
    {
	    va_start(arg,format);
	    pthread_mutex_lock(&log_lock);
	    vfprintf(logfile,format,arg);
	    pthread_mutex_unlock(&log_lock);
	    va_end(arg);
    }
#ifdef _TELNET_DEBUG_
	va_start(arg,format);
	vsnprintf(buff,512,format,arg);
	va_end(arg);
	printf(buff);
#endif
}
void wrt_log(char *src)
{
    pthread_mutex_lock(&log_lock);
	if(logfile)
	    fprintf(logfile,src);
	pthread_mutex_unlock(&log_lock);
#ifdef _TELNET_DEBUG_
    printf(src);
#endif
}
/*
  ������wrtlog,������д����־ǰ��д��"From <client ip address>"��������ͬ�ͻ�ʱд�����־
*/
void cwrtlog(struct session *sp,char *format,...)
{
    char buff[MAX_BUF_OUT];
    va_list args;
    sprintf(buff,"From %s:",sp->client_addr);
    va_start(args,format);
    vsnprintf(buff+strlen(buff),MAX_BUF_OUT-strlen(buff),format,args);
    va_end(args);
	wrt_log(buff);
}
char* peerip(struct session *sp,char *buf,int len)
{
    struct sockaddr_in addr;
    int addr_len;
    if(getpeername(sp->client_socket,(struct sockaddr*)&addr,&addr_len)!=0)
    {
        wrtlog("int client_wrtlog: bad socket:%d\n",sp->client_socket);
    }
    return (char*)inet_ntop(AF_INET,(void*)&addr.sin_addr,buf,len);
}
void telnet_ntoa(u_short src,char *buf,int len)//������ת��Ϊ�ַ���
{
    int i=0;
   //printf("src=%u\n",src);
    while(src!=0&&i<len)
    {
        buf[i]=(char)(0x30|(src%10));
	    src/=10;
	    i++;
    }
    int j=0;
    while(j<i/2)
    {
        buf[j]=buf[i-j-1]^buf[j];
	    buf[i-j-1]=buf[j]^buf[i-j-1];
	    buf[j]=buf[j]^buf[i-j-1];
	    j++;
    }
    buf[i]=0;
   //printf("buf=%s\n",buf);
}
void set_client_addr(struct session* sp,struct sockaddr_in *addr)//����session�еĿͻ��˵�ַ
{
    int len;
    inet_ntop(AF_INET,(void*)&addr->sin_addr,sp->client_addr,sizeof(sp->client_addr));
    len=strlen(sp->client_addr);
    memcpy(sp->client_addr+len,".",2);
    len++;
    telnet_ntoa(ntohs(addr->sin_port),sp->client_addr+len,sizeof(sp->client_addr)-len);
}
//��ʼ��״̬��
void fsminit(unsigned char trans_tab[][NCHARS],struct fsm fsm_trans[],int nstates)
{
	struct fsm *pf;
	int i,j,k;
	for(i=0;i<nstates;i++)
		for(j=0;j<NCHARS;j++)
		    trans_tab[i][j]=(unsigned char)INVALID;
	for(i=0;fsm_trans[i].current_state!=SINVALID;i++)
	{
		if(fsm_trans[i].input==CANY)
		{
			for(j=0;j<NCHARS;j++)
			{
				if(trans_tab[fsm_trans[i].current_state][j]==(unsigned char)INVALID)
				    trans_tab[fsm_trans[i].current_state][j]=(unsigned char)i;
			}
		}
		else
			trans_tab[fsm_trans[i].current_state][fsm_trans[i].input]=(unsigned char)i;
	}
	k=i;
	for(i=0;i<NCHARS;i++)
	{
		for(j=0;j<nstates;j++)
			if(trans_tab[j][i]==(unsigned char)INVALID)
		    {
			     trans_tab[j][i]=(unsigned char)(k);
		    }
	}
}
//��ʼ����״̬������ѡ��״̬��
void init_fsm()
{
	fsminit(trans_table,trans,NSTATES);
	fsminit(subtrans_table,subtrans,NSSTATES);
}
//ȥ��һ���ַ���ǰ��' ' '\t' '\n'���ַ�
void fre_strip(char *buf,int len,char **out)
{
	char del[]="\t\n\r ";
	int i=0;
	while(i<len&&strchr(del,buf[i])!=NULL)
	{
		i++;
	}
	*out=buf+i;
}
//ȥ��һ���ַ�����Ŀո�
void  back_strip(char *buf,int len,char **out)
{
	char del[]="\t\n\r ";
	int i=len-1;
	while(i>=0&&strchr(del,buf[i])!=NULL)
		i--;
	*out=buf+i+1;
}
//ɾ��һ��session sp
void clean(struct session *sp)
{
    if(!sp)
	{
        return;
	}
	shutdown(sp->client_socket,2);
    close(sp->client_socket);
    set_disp(sp,0);
    struct session *ss,*pre;
    ss=session_head;
    pre=session_head;
	pthread_mutex_lock(&var_lock);
    while(ss&&ss->thread_id!=sp->thread_id)
    {
        pre=ss;
        ss=ss->next;
    }
    if(!ss)
	    wrtlog("Can't find the client session.Should never happen here\n");
    else if(ss==session_head)
    {
        session_head=ss->next;
	    free(ss);
    }
    else
    {
	    pre->next=ss->next;
	    free(ss);
    }
    if(nclients>0)
        nclients--;
  // wrtlog("current clients number=%d\n",nclients);
    pthread_mutex_unlock(&var_lock);
    return;
}
//�������session,�رշ�����
void clean_all()
{
    struct session *sp,*np;
    sp=session_head;
    while(sp)
    {
        np=sp;
	    close(sp->client_socket);
	    sp->disp_ok=0;
	    sp=np->next;
	    free(np);
    }
    session_head=0;
    close(master_socket);
    close(fileno(logfile));
    pthread_exit(NULL);
}
//��ȡ��ǰsession
struct session* get_session()
{
    pthread_t tid;
    tid=pthread_self();
    struct session* sp;
    sp=session_head;
    while(sp&&sp->thread_id!=tid)sp=sp->next;
    return sp;
}
