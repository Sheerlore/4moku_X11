#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

// Connection Constant
#define BUFMAX 40
#define PORT_NO (u_short)20000
typedef enum
{
  SERVER = 1,
  CLIENT = 2,
} CONN_ROLE;

static int n, sofd, nsofd, retval;
static struct hostent *shost;
static struct sockaddr_in own, svaddr;
static char buf[BUFMAX], shostn[BUFMAX];
static fd_set mask;
static struct timeval tm;

// Window Constant
#define FIELD_W_NUM 7                                               // Number of Horizontal squares
#define FIELD_H_NUM 6                                               // Number of Vertical squares
const int WINDOW_W = 560;                                           // Window Width
const int WINDOW_H = 480;                                           // Window Height
const int WINDOW_PADDING = 40;                                      // Window padding
const int SQUARE_W = (WINDOW_W - WINDOW_PADDING * 2) / FIELD_W_NUM; // Square width
const int SQUARE_H = (WINDOW_H - WINDOW_PADDING * 2) / FIELD_H_NUM; // Square height
typedef enum
{
  EMPTY,
  WHITE,
  BLACK,
} KOMA;
int step; // even black, odd white

// Window Global var
Display *d;
Window w, ww;
XEvent event;
Font f;
GC gc;
XGCValues gv;

static int x, y;                                  // mouse x, y
static int x_r, y_r;                              // receive x, y
static int gap;                                   // koma gap(padding)
static int s_flag[FIELD_W_NUM][FIELD_H_NUM];      //Filed square flag(E or B or W)
static int s_lock_flag[FIELD_W_NUM][FIELD_H_NUM]; //Filed squeare lock flag
static int result;

int communication(CONN_ROLE role);
int ctoi(char c);
int put_koma(Display *d, Window w, GC gc, int wi, int hi, KOMA koma);
int update_lock_flag(int wi, int hi);
int check_field();

int main()
{

  d = XOpenDisplay(NULL);
  w = XCreateSimpleWindow(d, RootWindow(d, 0),
                          0, 0, WINDOW_W, WINDOW_H, 1,
                          BlackPixel(d, DefaultScreen(d)),
                          WhitePixel(d, DefaultScreen(d)));
  ww = XCreateSimpleWindow(d, w,
                           WINDOW_PADDING, WINDOW_PADDING,
                          //  WINDOW_W - WINDOW_PADDING * 2 + 1,
                          //  WINDOW_H - WINDOW_PADDING * 2 + 1, 1,
                          SQUARE_W * FIELD_W_NUM,
                          SQUARE_H * FIELD_H_NUM, 1,
                           BlackPixel(d, DefaultScreen(d)),
                           WhitePixel(d, DefaultScreen(d)));

  XSelectInput(d, ww, ButtonPressMask | ExposureMask);
  XMapWindow(d, w);
  XMapSubwindows(d, w);

  // Initialize flag
  for (int hi = 0; hi < FIELD_H_NUM; hi++)
  {
    for (int wi = 0; wi < FIELD_W_NUM; wi++)
    {
      s_flag[wi][hi] = EMPTY;
      s_lock_flag[wi][hi] = 1;
    }
  }
  for (int wi = 0; wi < FIELD_W_NUM; wi++)
  {
    s_lock_flag[wi][FIELD_H_NUM - 1] = 0;
  }
  x = 0;
  y = 0;
  gap = 10;
  step = 0;

  CONN_ROLE role;
  while (1)
  {
    printf("server: 1 client:2\n");
    scanf("%d", &role);
    if (role == SERVER || role == CLIENT)
      break;
  }
  if (role == SERVER)
  {
    XStoreName(d, w, "Server");
    printf("This is Server\n");
  }
  if (role == CLIENT)
  {
    XStoreName(d, w, "Client");
    printf("This is Client\n");
  }
  // start communication
  communication(role);
}

int communication(CONN_ROLE role)
{
  tm.tv_sec = 0;
  tm.tv_usec = 1;

  gc = XCreateGC(d, ww, 0, 0);
  f = XLoadFont(d, "fixed");
  XSetFont(d, gc, f);

  if (role == SERVER)
  {
    if (gethostname(shostn, sizeof(shostn)) < 0)
    {
      perror("gethostname");
      exit(1);
    }
    printf("hostname is %s\n", shostn);

    shost = gethostbyname(shostn);
    if (shost == NULL)
    {
      perror("gethostbyname");
      exit(1);
    }

    bzero((char *)&own, sizeof(own));
    own.sin_family = AF_INET;
    own.sin_port = htons(PORT_NO);
    bcopy(
        (char *)shost->h_addr,
        (char *)&own.sin_addr,
        shost->h_length);

    sofd = socket(AF_INET, SOCK_STREAM, 0);
    if (sofd < 0)
    {
      perror("socket");
      exit(1);
    }
    if (bind(sofd, (struct sockaddr *)&own, sizeof(own)) < 0)
    {
      perror("bind");
      exit(1);
    }
    listen(sofd, 1);
    nsofd = accept(sofd, NULL, NULL);
    if (nsofd < 0)
    {
      perror("accept");
      exit(1);
    }
    close(sofd);
    write(1, "START\n", 6);
  }

  if (role == CLIENT)
  {
    printf("hostname ?: ");
    scanf("%s", shostn);
    printf("\n");

    shost = gethostbyname(shostn);
    if (shost == NULL)
    {
      perror("gethostbyname");
      exit(1);
    }

    bzero((char *)&svaddr, sizeof(svaddr));
    svaddr.sin_family = AF_INET;
    svaddr.sin_port = htons(PORT_NO);
    bcopy(
        (char *)shost->h_addr,
        (char *)&svaddr.sin_addr,
        shost->h_length);

    sofd = socket(AF_INET, SOCK_STREAM, 0);
    if (sofd < 0)
    {
      perror("socket");
      exit(1);
    }
    connect(sofd, (struct sockaddr *)&svaddr, sizeof(svaddr));
    write(1, "START\n", 6);
  }

  while (1)
  {
    if (role == SERVER)
    {
      FD_ZERO(&mask);
      FD_SET(nsofd, &mask);
      FD_SET(0, &mask);
      retval = select(nsofd + 1, &mask, NULL, NULL, &tm);
    }
    if (role == CLIENT)
    {
      FD_ZERO(&mask);
      FD_SET(sofd, &mask);
      FD_SET(0, &mask);
      retval = select(sofd + 1, &mask, NULL, NULL, &tm);
    }
    if (retval < 0)
    {
      perror("select");
      exit(1);
    }

    if (XPending(d) != 0)
    {
      XNextEvent(d, &event);
      switch (event.type)
      {
      case Expose:
        // Draw Field
        for (int hi = 0; hi < FIELD_H_NUM; hi++)
        {
          for (int wi = 0; wi < FIELD_W_NUM; wi++)
          {
            XDrawRectangle(d, ww, gc,
                           wi * SQUARE_W, hi * SQUARE_H,
                           SQUARE_W, SQUARE_H);
            if (s_flag[wi][hi] == BLACK)
            {
              put_koma(d, ww, gc, wi, hi, BLACK);
            }
            else if (s_flag[wi][hi] == WHITE)
            {
              put_koma(d, ww, gc, wi, hi, WHITE);
            }
          }
        }
        break;

      case ButtonPress:
        x = event.xbutton.x;
        y = event.xbutton.y;
        for (int hi = 0; hi < FIELD_H_NUM; hi++)
        {
          for (int wi = 0; wi < FIELD_W_NUM; wi++)
          {
            if (wi * SQUARE_W <= x && x <= (wi + 1) * SQUARE_W &&
                hi * SQUARE_H <= y && y <= (hi + 1) * SQUARE_H &&
                s_lock_flag[wi][hi] != 1)
            {
              if (role == SERVER && step % 2 == 0)
              {
                put_koma(d, ww, gc, wi, hi, BLACK);
                // send to client
                write(1, "        ", 8);
                snprintf(buf, BUFMAX, "PLACE-%d%d\0", wi, hi);
                n = sizeof(buf);
                write(nsofd, buf, n);

                write(1, buf, n);
                write(1, " :SERVER(OWN)\n", 15);

                update_lock_flag(wi, hi);
                step++;
              }
              else if (role == CLIENT && step % 2 != 0)
              {
                put_koma(d, ww, gc, wi, hi, WHITE);
                // send to server
                write(1, "        ", 8);
                snprintf(buf, BUFMAX, "PLACE-%d%d\0", wi, hi);
                n = sizeof(buf);
                write(sofd, buf, n);

                write(1, buf, n);
                write(1, " :CLIENT(OWN)\n", 15);

                update_lock_flag(wi, hi);
                step++;
              }
            }
          }
        }
        break;
      }
    }

    // receive message from client
    if (role == SERVER && FD_ISSET(nsofd, &mask))
    {
      n = read(nsofd, buf, BUFMAX);
      printf("buf: %s\n", buf);
      write(1, "CLIENT> ", 8);
      if (strcmp(buf, "YOU-WIN") == 0)
      {
        write(1, buf, n);
        write(1, "\n", 2);
        break;
      }

      if (strcmp(buf, "ERROR") == 0)
      {
        write(1, buf, n);
        write(1, "\n", 2);
        break;
      }

      write(1, buf, n);
      write(1, "\n", 2);
      x_r = ctoi(buf[6]);
      y_r = ctoi(buf[7]);
      if (x_r != -1 && y_r != -1)
      {
        put_koma(d, ww, gc, x_r, y_r, WHITE);
        update_lock_flag(x_r, y_r);
        result = check_field();
        if (result == WHITE)
        {
          write(nsofd, "YOU-WIN\0", 9);
          break;
        }
        step++;
      }
      bzero(buf, BUFMAX);
    }

    // receive message from server 
    if (role == CLIENT && FD_ISSET(sofd, &mask))
    {
      n = read(sofd, buf, BUFMAX);
      printf("buf: %s\n", buf);
      write(1, "SERVER> ", 8);

      if (strcmp(buf, "YOU-WIN") == 0)
      {
        write(1, buf, n);
        write(1, "\n", 2);
        break;
      }

      if (strcmp(buf, "ERROR") == 0)
      {
        write(1, buf, n);
        write(1, "\n", 2);
        break;
      }

      write(1, buf, n);
      write(1, "\n", 2);
      x_r = ctoi(buf[6]);
      y_r = ctoi(buf[7]);
      if (x_r != -1 && y_r != -1)
      {
        put_koma(d, ww, gc, x_r, y_r, BLACK);
        update_lock_flag(x_r, y_r);
        result = check_field();
        if (result == BLACK)
        {
          write(sofd, "YOU-WIN\0", 9);
          break;
        }
        step++;
      }
      bzero(buf, BUFMAX);
    }
  }
  if (role == SERVER)
    close(nsofd);
  if (role == CLIENT)
    close(sofd);
}

// convert char to int
int ctoi(char c)
{
  if (c >= '0' && c <= '9')
  {
    return c - '0';
  }
  return -1;
}

// put koma on field
int put_koma(Display *d, Window w, GC gc, int wi, int hi, KOMA koma)
{
  if (koma == BLACK)
  {
    XFillArc(d, ww, gc,
             wi * SQUARE_W + gap, hi * SQUARE_H + gap,
             SQUARE_W - gap * 2, SQUARE_H - gap * 2,
             0 * 64, 360 * 64);
    s_flag[wi][hi] = BLACK;
  }

  if (koma == WHITE)
  {
    XDrawArc(d, ww, gc,
             wi * SQUARE_W + gap, hi * SQUARE_H + gap,
             SQUARE_W - gap * 2, SQUARE_H - gap * 2,
             0 * 64, 360 * 64);
    s_flag[wi][hi] = WHITE;
  }
}

// update lock flag
int update_lock_flag(int wi, int hi)
{
  s_lock_flag[wi][hi] = 1;
  if (hi != 0)
  {
    s_lock_flag[wi][hi - 1] = 0;
  }
}

// check 4moku
// return B or W or -1
int check_field()
{
  const int count_num = 4;
  int step;
  int b_count, w_count;
  int hi, wi;
  // Check the horizontal direction
  for (hi = 0; hi < FIELD_H_NUM; hi++)
  {
    for (wi = 0; wi < FIELD_W_NUM - (count_num - 1); wi++)
    {
      step = 0;
      b_count = w_count = 0;
      while (step < count_num)
      {
        if (s_flag[wi + step][hi] == BLACK)
          b_count++;
        if (s_flag[wi + step][hi] == WHITE)
          w_count++;
        if (b_count > 0 && w_count > 0)
          break;
        step++;
      }
      if (b_count == count_num)
        return BLACK;
      if (w_count == count_num)
        return WHITE;
    }
  }
  // Check the vertical direction
  for (wi = 0; wi < FIELD_W_NUM; wi++)
  {
    for (hi = 0; hi < FIELD_H_NUM - (count_num - 1); hi++)
    {
      step = 0;
      b_count = w_count = 0;
      while (step < count_num)
      {
        if (s_flag[wi][hi + step] == BLACK)
          b_count++;
        if (s_flag[wi][hi + step] == WHITE)
          w_count++;
        if (b_count > 0 && w_count > 0)
          break;
        step++;
      }
      if (b_count == count_num)
        return BLACK;
      if (w_count == count_num)
        return WHITE;
    }
  }
  // Check the diagonally opposite right
  // Check the diagonally opposite left
  return -1;
}