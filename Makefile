CC=gcc
CXX=g++
CFLAGS=-DMG_ENABLE_THREADS -pthread -O3
CPPFLAGS=-std=c++11 -O3 -g `mysql_config --include` -pthread

SRCS_C=mongoose.c
SRCS_CXX=tsources.cpp wikidata_db.cpp tools.cpp tplatform.cpp tpagelist.cpp trenderer.cpp

OBJS=$(subst .c,.o,$(SRCS_C)) $(subst .cpp,.o,$(SRCS_CXX))

all: testing server

server: $(OBJS)
	g++ main.cpp $(OBJS) -o petscan -O3 $(CPPFLAGS) -ldl `mysql_config --libs` -l curl

testing: $(OBJS)
	g++ testing.cpp $(OBJS) -o testing -O3 $(CPPFLAGS) -ldl `mysql_config --libs` -l curl

clean:
	\rm -f $(OBJS) petscan testing

%.o: %.cpp
	$(CXX) $(CPPFLAGS) -c $<

%.o: %.c
	$(CC) $(CFLAGS) -c $<
