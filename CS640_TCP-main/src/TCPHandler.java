import java.net.InetAddress;
import java.net.DatagramSocket;
import java.net.DatagramPacket;
import java.net.SocketException;
import java.io.IOException;
import java.text.DecimalFormat;

// Sender and receiver 共同的功能
public abstract class TCPHandler extends Thread {
  protected static final boolean DE_BUG = false;
  // Host 需用用來管理packet的資訊
  DatagramSocket socket;
  InetAddress remoteIP;
  int remotePort;
  String fileName;
  int mtu;
  int sws;
  int seqNum;
  int ackNum;
  int lastRecAck;
  int duplicateAckCount = 0;
  long startTime;

  // Flag用來控制thread＝＝＝＝＝＝＝＝＝＝＝＝
  private boolean needToStop = false;
  volatile boolean established = false;
  boolean initiatedClose;
  boolean waitingForClose;
  boolean syn_rec;
  boolean timeWait;

  TimerManager timerManager;
  // 統計資料使用＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝
  int dataTransferred;
  int packetsSent;
  int packetsReceived;
  int outOfSeqPackets;
  int incorrectChecksumNum;
  int retransNum;
  int duplicateAcksNum;

  //flags for fin
  boolean firstFin = false;
  boolean firstReceiverFin = false;




  public synchronized int getSeqNum() {
    return seqNum;
  }

  public synchronized void setSeqNum(int seqNum) {
    this.seqNum = seqNum;
  }

  public synchronized int getAckNum() {
    return ackNum;
  }

  public synchronized void setAckNum(int ackNum) {
    this.ackNum = ackNum;
  }

  public TCPHandler(int port, String fileName, int mtu, int sws) {
    timerManager = new TimerManager(this);
    try {
      this.socket = new DatagramSocket(port);
    } catch (SocketException e) {
      System.out.println("debug: Socket could not be created");
    }
    this.mtu = mtu;
    this.sws = sws;
    this.fileName = fileName;
    this.startTime = System.nanoTime();

  }

  public void run() {
    // host A 向 host B 發出第一次握手
    if (remoteIP != null) {
      this.socket.connect(remoteIP, remotePort);
      sendTCP(new byte[0], new Boolean[] { true, false, false }); // {S - - }
    }
    // 持續接收數據
    while (running()) {
      byte[] data = new byte[mtu];
      DatagramPacket receivedDatagram = new DatagramPacket(data, data.length);

      try {
        this.socket.receive(receivedDatagram);
        //===============================收到包裹了===============================
        packetsReceived += 1;
        TCPpacket receivedPacket = new TCPpacket();
        // checksum錯誤 -> 直接不予理會
        if (receivedPacket.verifyChecksum(receivedDatagram.getData()) == false) {
          incorrectChecksumNum += 1;
          System.out.println("debug: Incorrect checksum");
          return;
        }
        // 打印數據包
        hostOutput(receivedPacket, "rcv");

        // 收到ack包
        if (receivedPacket.getAck()) {
          //收到ack包的後續處理
          if (!receivedPacket.getFin() && waitingForClose) {
            endThread();
            return;
          }

          if (receivedPacket.ackNum > lastRecAck) {
            lastRecAck = receivedPacket.ackNum;
            duplicateAckCount = 0;
            timerManager.updateTO(receivedPacket);
            timerManager.removePacket(receivedPacket.ackNum);
          } else if (receivedPacket.ackNum == lastRecAck) {
            duplicateAckCount += 1;
            duplicateAcksNum += 1;
            if (duplicateAckCount == 3)
              timerManager.resendPacket(receivedPacket);
          }
        }

        // 收到ＳＹＮ包
        if (receivedPacket.getSyn()) {
          // Set up socket if it isn't connected
          if (this.socket.isConnected() == false) {
            remoteIP = receivedDatagram.getAddress();
            remotePort = receivedDatagram.getPort();
            this.socket.connect(remoteIP, remotePort);
          }
          // 收到ＳＹＮ包後的後續處理
          // receiver
          if (receivedPacket.getAck() == false) {
            if (syn_rec == false) {
              //ackNum = receivedPacket.seqNum + 1;
              setAckNum(receivedPacket.seqNum + 1);
              if (DE_BUG) System.out.println("handler 134 ack number : " + getAckNum());
              sendTCP(new byte[0], new Boolean[] { true, false, true }, receivedPacket.timestamp);
              syn_rec = true;
            }
          }
          // sender
          else {
            //ackNum = receivedPacket.seqNum + 1;
            setAckNum(receivedPacket.seqNum + 1);
            if(DE_BUG) System.out.println("handler 142 ack number: " + getAckNum());
            if (established)
              resendACKtoSYN();
            else {
              sendACK(receivedPacket);
              established = true;
            }
          }
        }
        // 收到ＦＩＮ包
        else if (receivedPacket.getFin()) {
          // 更新ＡＣＫＮＵＭ
          if (receivedPacket.seqNum == getAckNum()) {
            //ackNum += 1;
            if(!firstFin) {
              setAckNum(getAckNum() + 1);
              firstFin = true;
              if (DE_BUG) System.out.println("handler 157 ack number: " + getAckNum());
            }
          }
          // 接收到ＦＩＮ包後的後續處理
          // receiver
          if (initiatedClose == false) {
            if(!firstReceiverFin){
              sendFinAck(); // FIN+ACK包{ - F A}
              waitingForClose = true;
              firstReceiverFin = true;
            }
//            waitingForClose = true;
//            firstReceiverFin = true;
          }
          // sender
          else {
            sendACK(receivedPacket);
            if (timeWait == false) {
              WaitClose closeThreadTimer = new WaitClose(this, (int) timerManager.getTO() / 1000000 * 4);
              closeThreadTimer.start();
              timeWait = true;
            }
          }
        }
        // 收到ＤＡＴＡ包
        else if (receivedPacket.data.length > 0) {
          timerManager.removePacket(receivedPacket.ackNum);
          handlePacket(receivedPacket);
          if(receivedPacket.seqNum <= getAckNum()) sendACK(receivedPacket); //receivedPacket只用來調時間 其他參數照現存的數值回傳
        }
      } catch (IOException e) {
      }
    }
  }
  //給TCPsender overwrite用：用來處理DATA包
  abstract void handlePacket(TCPpacket packet);

  //===================================傳輸packet的方法=============================================================
  void sendACK(TCPpacket origPacket) {
    sendTCP(new byte[0], new Boolean[] { false, false, true }, origPacket.timestamp); //{ - - A}
  }

  void resendACKtoSYN() {
    long time = System.nanoTime();
    TCPpacket tcpPacket = new TCPpacket(1, getAckNum(), time, new Boolean[] { false, false, true }, new byte[0]);
    sendTCP(tcpPacket);
  }

  void sendFinAck() {
    sendTCP(new byte[0], new Boolean[] { false, true, true }); // { - F A}
  }

  void sendTCP(byte[] data, Boolean[] flags) {
    long time = System.nanoTime();
    sendTCP(data, flags, time);
  }

  void sendTCP(byte[] data, Boolean[] flags, long time) {
    TCPpacket tcpPacket = new TCPpacket(getSeqNum(), getAckNum(), time, flags, data);
    sendTCP(tcpPacket);

    //需要等待ＡＣＫ回應的包 -> 加入buffer中等待被ＡＣＫ
    if (tcpPacket.getSyn() || tcpPacket.getFin() || tcpPacket.data.length > 0) {
      timerManager.startTimerOnPacket(tcpPacket, 0);
    }

    //需要佔用seqNum的數據包
    if (tcpPacket.getSyn() || tcpPacket.getFin() || tcpPacket.isDataPacket()) {
      //seqNum += Math.max(1, data.length); //1 for sys or fin packet
      setSeqNum(getSeqNum() + Math.max(1, data.length)); //1 for sys or fin packet
    }
  }

  synchronized void sendTCP(TCPpacket tcpPacket) {
    byte[] tcpData = tcpPacket.serialize();
    DatagramPacket packet = new DatagramPacket(tcpData, tcpData.length, remoteIP, remotePort);
    try {
      hostOutput(tcpPacket, "snd");
      this.socket.send(packet);
      packetsSent += 1;
    } catch (IOException e) {
      System.out.println("debug: send packet not success");
    }
  }

  public void resendTCPPacket(TCPpacket tcpPacket) {
    retransNum += 1;
    tcpPacket.timestamp = System.nanoTime();
    tcpPacket.ackNum = getAckNum();

    sendTCP(tcpPacket);
  }

  void hostOutput(TCPpacket tcpPacket, String sndOrec) {
    long time = (System.nanoTime() - startTime) / ((long) 1e6);
    DecimalFormat df = new DecimalFormat("##.###");

    System.out.println(sndOrec + " " + df.format((double) time / 1e3) + " " + (tcpPacket.getSyn() ? "S " : "- ")
        + ((tcpPacket.getAck() || tcpPacket.isDataPacket()) ? "A " : "- ") +
        (tcpPacket.getFin() ? "F " : "- ") + (tcpPacket.data.length > 0 ? "D " : "- ") + tcpPacket.seqNum +
        " " + tcpPacket.data.length + " " + tcpPacket.ackNum);
  }

  public synchronized void endThread() {
    this.needToStop = true;
    closeConnection();

    established = true;
    printStatistics();
  }

  protected synchronized boolean running() {
    return this.needToStop == false ? true : false;
  }

  public void closeConnection() {
    timerManager.removeAllPacket();
    socket.close();
  }

  void printStatistics() {
    System.out.println("-------------------------------------------");
    System.out.println("Amount of Data transferred: " + dataTransferred);
    System.out.println("Number of packets sent/received: " + packetsSent + "/" + packetsReceived);
    System.out.println("Number of out of sequence packets: " + outOfSeqPackets);
    System.out.println("Number of packets discarded due to incorrect checksum: " + incorrectChecksumNum);
    System.out.println("Number of packets retransmitted: " + retransNum);
    System.out.println("Number of duplicate ACKs: " + duplicateAcksNum);
    System.out.println("-------------------------------------------");
  }
}
