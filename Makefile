CC=clang++-18
CFLAGS=-std=c++20 -stdlib=libc++ -O3 

OBJ = ArchitectureBuilder.o Architecture.o cloud_calc.o AuroraArchitecture.o SocratesArchitecture.o InMemArchitecture.o LogService.o PageService.o RemoteBlockDeviceArchitecture.o ClassicArchitecture.o DynamicArchitecture.o HADRArchitecture.o MetricRegistry.o Metric.o Metrics.o Resources.o infra/Parser.o infra/CSV.o infra/ArgumentParser.o infra/File.o

%.o: %.cpp
	$(CC) -c -o $@ $< $(CFLAGS)

cloud_calc: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f *.o *~ core infra/*.o
