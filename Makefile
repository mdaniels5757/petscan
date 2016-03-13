CC=gcc
CXX=g++
CFLAGS=-DMG_ENABLE_THREADS -pthread
CPPFLAGS=-std=c++11  -g `mysql_config --include`

SRCS_C=mongoose.c
SRCS_CXX=myjson.cpp tsources.cpp

OBJS=$(subst .c,.o,$(SRCS_C)) $(subst .cpp,.o,$(SRCS_CXX))

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
