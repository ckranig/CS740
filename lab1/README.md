Group:
Wei-Shiang Wung: wwung, Calvin Kranig: ckranig, Ying-Fang Jaw: yjaw

For this project we assume a fixed window size for each flow and that the correct MAC address has been input in line 299 of lab1-client.c.

To run start the make each file and follow the following steps:
1. On the server and client host run: `make & cd build`
2. On the server host run: sudo `./lab1-server`
3. On the client host run: `sudo ./lab1-client <flow_num> <flow_size>`