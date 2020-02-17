## Steps to run
```
make
bin/server <port>
bin/client <serv_ip> <port> <name> <gno>
```
## Measurements
The following table summarizes the average delay between two clients with the number of clients on the LHS.

| #clients | Time (ms)  |
|----------|------------|
| 10       | 1.2198242  |
| 20       | 1.3266114  |
| 30       | 1.4750002  |
| 40       | 1.7257812  |
| 50       | 2.5587892  |
| 60       | 3.0399902  |
| 70       | 3.2807618  |
| 80       | 3.5618652  |
| 90       | 3.8848146  |
| 100      | 3.73028575 |
