/*
Linux 提供的 inotify 是一种异步文件监控机制，可以用来监控文件系统的事件，包括访问、读写、权限、删除、移动等事件

相关结构体inotify_event
    struct inotify_event {
        int      wd;       监控对象的watch描述符
        uint32_t mask;     事件掩码
        uint32_t cookie;   和rename事件相关
        uint32_t len;      name字段的长度
        char     name[];   监控对象的文件或目录名
    };

mask事件定义:
#define IN_ACCESS	 0x00000001
#define IN_MODIFY	 0x00000002
#define IN_ATTRIB	 0x00000004
#define IN_CLOSE_WRITE	 0x00000008
#define IN_CLOSE_NOWRITE 0x00000010
#define IN_CLOSE	 (IN_CLOSE_WRITE | IN_CLOSE_NOWRITE)
#define IN_OPEN		 0x00000020
#define IN_MOVED_FROM	 0x00000040
#define IN_MOVED_TO      0x00000080
#define IN_MOVE		 (IN_MOVED_FROM | IN_MOVED_TO)
#define IN_CREATE	 0x00000100
#define IN_DELETE	 0x00000200
#define IN_DELETE_SELF	 0x00000400
#define IN_MOVE_SELF	 0x00000800
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <time.h>
#include <dirent.h>
#include <malloc.h>
#include <string.h>
#include <sys/types.h>

#define SHARE_DUR "/home/montana/桌面"
#define EVENT_NUM 12
#define DATA_W 1024

static struct dir_path{
    int id;
    char **path;
}dir;

char *event_str[12] = {
    "IN_ACCESS",        //文件被访问
    "IN_MODIFY",        //文件修改
    "IN_ATTRIB",        //原数据被改变，例如权限、时间戳、扩展属性、链接数、UID、GID等
    "IN_CLOSE_WRITE",   
    "IN_CLOSE_NOWRITE",
    "IN_OPEN",          //文件或目录被打开
    "IN_MOVED_FROM",    //从监控的目录中移出文件
    "IN_MOVED_TO",      //向监控的目录中移入文件
    "IN_CREATE",        //在监控的目录中创建了文件或目录
    "IN_DELETE",        //在监控的目录中删除了文件或目录
    "IN_DELETE_SELF",   //监控的文件或目录本身被删除
    "IN_MOVE_SELF"      //监控的文件或目录本身被移动
};

int id_add(const char *path_id){
        dir.path[dir.id]=(char *)malloc(DATA_W);
        strncpy(dir.path[dir.id],path_id,DATA_W);
        printf("监控文件夹%d:绝对路径为%s\n",dir.id,dir.path[dir.id]);
        dir.id=dir.id+1;
        return 0;
}

int inotify_watch_dir(const char *dir_path,int fd){
    int wd;
    int len;
    DIR *dp;
    char pdir_home[DATA_W];
    char pdir[DATA_W];
    strcpy(pdir_home,dir_path);
    struct dirent *dirp;
    struct inotify_event *event;
    if (fd < 0)
    {
        fprintf(stderr, "inotify_init failed\n");
        return -1;
    }
    //函数inotify_add_watch用于添加一个监控列表，返回unique的watch描述符，返回值用来判断返回的事件属于哪个监听的文件
    wd = inotify_add_watch(fd, dir_path, IN_CREATE|IN_ATTRIB|IN_DELETE|IN_MOVED_FROM|IN_MOVED_TO);
    if ((dp = opendir(dir_path)) == NULL)
    {
        return -1;
    }
    while ((dirp = readdir(dp)) != NULL)
    {
            if(strcmp(dirp->d_name,".") == 0 || strcmp(dirp->d_name,"..") == 0){
                continue;
            }
            if (dirp->d_type == DT_REG){    //如果为普通文件
                continue;
            }
            if (dirp->d_type == DT_DIR){    //如果为文件目录
                strncpy(pdir,pdir_home,DATA_W);
                //strncat函数可以实现将从参数2开始的前num个元素追加到参数1末尾,参数1的'\0'会被参数2覆盖;当num大于字符串参数2的长度，那就只追加参数2中'\0'前面的部分
                strncat(pdir,"/",2);//由于num刚好等于参数2的长度时编译会报警告，因此让num等于num长度加1
                strncat(pdir,dirp->d_name,BUFSIZ);
                id_add(pdir);
                inotify_watch_dir(pdir, fd);
                /*
                inotify 既可以监控单个文件，也可以监控整个目录，当监控的对象是一个目录的时候，目录本身和目录下的文件/文件夹都是被监控的对象
                但是不能递归监控子目录，如果想要监控子目录下的文件，需要自己通过递归的方法将所有子目录都添加到监控中
                */
            }
        }
        closedir(dp);
}

void *scan_inotify(void *input){
    const char *scan_dir = (char*)input;
    printf("scan_dir = %s\n",scan_dir);

    #if 1
    int i,len,nread;
    struct stat res;
    char path[BUFSIZ];
    char buf[BUFSIZ];
    struct inotify_event *event;
    char log_dir[DATA_W] = {0};     //log文件的绝对路径
    dir.id =1;
    dir.path = (char **)malloc(65530);
    id_add(scan_dir);

    int fd = inotify_init();    //创建一个inotify的实例，然后返回inotify事件队列的文件描述符
    inotify_watch_dir(scan_dir,fd);
    getcwd(log_dir,DATA_W);                 //getcwd()会将当前工作目录的绝对路径复制到参数log_dir所指的内存空间中,参数DATA_W为buf的空间大小。
    strncat(log_dir,"/inotify.log",13);
    printf("文件变化的记录写入log文件%s\n\n",log_dir);
    buf[sizeof(buf) - 1] = 0;
    while((len = read(fd,buf,(sizeof(buf) - 1))) > 0){
        //read的返回值存在一个或者多个inotify_event对象，需要一个一个取出来处理
        nread = 0;
        while(len > 0){
            event = (struct inotify_event *) &buf[nread];//取出数组buf的元素nread的地址给到event
            for(i=0;i<EVENT_NUM;i++){
                if((event->mask >> i) & 1){
                    //std::cout<<"event->mask >> "<< i <<" = " << event->mask<<std::endl;
                    if(event->len > 0){
                        if(strncmp(event->name,".",1)){ //如果不为隐藏文件
                            FILE *fp = NULL;
                            fp = fopen(log_dir,"a");
                            fprintf(fp,"File: %s/%s --- %s\n",dir.path[event->wd],event->name,event_str[i]);        //将变化记录写进log文件
                            printf("文件%s/%s 发生改动 --- 改动事件为%s\n",dir.path[event->wd],event->name,event_str[i]);

                            //如果监控目录内新建或者移进了新文件，判断其是否为一个目录，如果是就将此目录加入监控列表
                            if((!strcmp(event_str[i],"IN_CREATE")) | (!strcmp(event_str[i],"IN_MOVED_TO"))){
                                memset(path,0,sizeof path);
                                strncat(path,dir.path[event->wd],BUFSIZ);
                                strncat(path,"/",2);
                                strncat(path,event->name,BUFSIZ);
                                stat(path ,&res);
                                if(S_ISDIR(res.st_mode)){
                                    inotify_add_watch(fd, path, IN_CREATE|IN_ATTRIB|IN_DELETE|IN_MOVED_FROM|IN_MOVED_TO);   
                                    id_add(path);
                                }
                            }
                        }
                    }
                }
            }
            //nread偏移量的处理，一个事件的大小 = inotify_event 结构体所占用的空间 + inotify_event->name 所占用的空间
            nread = nread + sizeof(struct inotify_event) + event->len;
            len = len - sizeof(struct inotify_event) - event->len;
        }
    }
    #endif
}

int main(int argc, char const *argv[])
{
    if(argc < 2){
        printf("请传入要监听的路径!\n");
    }
    const char *scan_dir = argv[1];

    scan_inotify((void *)scan_dir);
    
    return 0;
}
