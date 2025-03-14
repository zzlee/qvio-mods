# cancel impicit rules
%: %.o

%.c.o: %.c
	@echo "CC: $< ==> $@"
	$(AT)$(CC) $(CFLAGS) -fPIC -c $< -o $@

%.cpp.o: %.cpp
	@echo "CXX: $< ==> $@"
	$(AT)$(CXX) $(CXXFLAGS) -fPIC -c $< -o $@

%.cu.o: %.cu
	@echo "NVCC: $< ==> $@"
	$(AT)$(NVCC) $(CUFLAGS) -c $< -o $@
