import sys

input_file = sys.argv[1]
flow_size = int(sys.argv[2])

cnt_flow_num = 0
fct_sum = 0
with open(input_file, "r") as f:
    for line in f:
        if line.startswith("Flow"):
            tokens = line.strip("\n").split(" ")
            if True:
            # if int(tokens[4]) > flow_size:
                cnt_flow_num += 1
                fct_sum += float(tokens[10])
                # if cnt_flow_num < 15:
                #     print(cnt_flow_num, float(tokens[10]))
    print(fct_sum/cnt_flow_num)