# Makefile for DriverTest
CXX = g++
CXXFLAGS = -std=c++11 -Wall -I.
LDFLAGS = -lksuser -lole32 -luuid 
# ksuser.lib for KSPROPERTY, ole32/uuid for GUIDFromString if used for SetupAPI

TARGET = DriverTest.exe
SOURCES = DriverTest.cpp WavWriter.cpp 
OBJECTS = $(SOURCES:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)

%.o: %.cpp WavWriter.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
