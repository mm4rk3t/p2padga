default:
	mkdir -p bin/ && g++ main.cpp -lboost_system -lpthread -lboost_thread -ltorrent-rasterbar -L/usr/local/lib -lncurses -o bin/p2padga
