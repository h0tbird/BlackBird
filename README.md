# BlackBird 0.0.1
High-performance, high-scalable, epoll-based, edge-triggered, non-blocking, pre-threaded and multiplexed generic TCP server skeleton.

##Design
**Diagram**

                               FIFO
          +···········································+
    +---->| 1 | 2 | 3 | 4 | · | · | · | · | · | · | · |<----+
    |     +···········································+     |    +---+
    |           |       |       |       |       |           |    | M |  Main thread.
    |         +---+   +---+   +---+   +---+   +---+         |    +---+
    |         | D |   | D |   | D |   | D |   | D |         |
    |         +---+   +---+   +---+   +---+   +---+         |    +---+
    |        \__________________ __________________/        |    | A |  Accept worker.
    |                           V                           |    +---+
    |                         +---+                         |
    |       CORE1 <-----------| M |-----------> CORE2       |    +---+
    |  _______^_______        +---+        _______^_______  |    | W |  Wait worker.
    | /               \         |         /               \ |    +---+
    |     *······*              V              *······*     |
    |   +-| epfd |           *·····*           | epfd |-+   |    +---+
    |   | *······*           | sfd |           *······* |   |    | D |  Data worker.
    |   |     ^              *·····*              ^     |   |    +---+
    |   |     |  +---+          |          +---+  |     |   |
    |   V     +--| A |<-[1]-----+-----[2]->| A |--+     V   |
    | +---+   |  +---+          |          +---+  |   +---+ |
    +-| W |   |                 |                 |   | W |-+
      +---+   |  +---+          |          +---+  |   +---+
              +--| A |<-[3]-----+-----[4]->| A |--+
                 +---+                     +---+  
     _________^___________________________________^_________
    /                                                       \
      *···*   *···*   *···*   *···*   *···*   *···*   *···*
      | 1 |   | 2 |   | 3 |   | 4 |   | · |   | · |   | · |
      *···*   *···*   *···*   *···*   *···*   *···*   *···*

##Install
**CentOS:**
    git clone git@github.com:h0tbird/BlackBird.git
    mv BlackBird BlackBird-0.0.1
    tar cf BlackBird-0.0.1.tar BlackBird-0.0.1
    gzip BlackBird-0.0.1.tar
    rpmbuild -ta BlackBird-0.0.1.tar.gz
    rpm -Uvh /usr/src/redhat/RPMS/x86_64/BlackBird-0.0.1-1.el5.x86_64.rpm

##License
See the file `COPYING`
