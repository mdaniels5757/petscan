CC=gcc-6
CXX=g++-6
CFLAGS=-DMG_ENABLE_THREADS -pthread -O3
CPPFLAGS=-std=c++14 -O3 -g `mysql_config --include` -pthread

SRCS_C=mongoose.c
SRCS_CXX=tsources.cpp wikidata_db.cpp tools.cpp tplatform.cpp tpagelist.cpp trenderer.cpp twdfist.cpp

OBJS=$(subst .c,.o,$(SRCS_C)) $(subst .cpp,.o,$(SRCS_CXX))

TARGET=petscan

all: server
# testing

server: $(TARGET)

$(TARGET): $(OBJS)
	${CXX} main.cpp $(OBJS) -o $(TARGET) -O3 $(CPPFLAGS) -ldl `mysql_config --libs` -l curl

testing: $(OBJS)
	${CXX} testing.cpp $(OBJS) -o testing -O3 $(CPPFLAGS) -ldl `mysql_config --libs` -l curl

clean:
	\rm -f $(OBJS) $(TARGET) testing

%.o: %.cpp
	$(CXX) $(CPPFLAGS) -c $<

%.o: %.c
	$(CC) $(CFLAGS) -c $<
