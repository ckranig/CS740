import java.io.FileOutputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.PriorityQueue;

// Receives file from sender using TCP from base class
public class Receiver extends TCPHandler {
  PriorityQueue<TCPpacket> receiverBuffer;
  FileOutputStream fileOutputStream;

  public Receiver(int port, int mtu, int sws, String fileName){
    super(port, fileName, mtu, sws);
    receiverBuffer = new PriorityQueue<TCPpacket>((packet1, packet2) -> packet1.seqNum - packet2.seqNum);

    try{
      fileOutputStream = new FileOutputStream(fileName);
    }catch(FileNotFoundException ex){
      System.out.println(ex.getMessage());
    }
  }

  public boolean inBuffer(TCPpacket packet){
    for (TCPpacket p : receiverBuffer){
      if(p.seqNum == packet.seqNum)
        return true;
    }
    return false;
  }

  public void handlePacket(TCPpacket packet){
    byte[] data = packet.data;
    switch (Integer.compare(packet.seqNum, getAckNum())) {
      case -1: // packet.seqNum < ackNum 這個包裹之前已經收過：丟棄
        //outOfSeqPackets += 1;
        break;
      case 1: // packet.seqNum > ackNum 這個包裹超前了：先緩存起來
        outOfSeqPackets += 1;
        if (inBuffer(packet)) return;// check for packet already in buffer
        receiverBuffer.add(packet);// add packet into buffer if not in there
        break;
      case 0: // packet.seqNum == ackNum 正確的包裹：更新ackNum並處理
        //ackNum = packet.seqNum + packet.data.length;
        setAckNum(packet.seqNum + packet.data.length);
        if(DE_BUG) System.out.println("receiver 46 ack number" + getAckNum() );
        dataTransferred += packet.data.length;

        try {
          fileOutputStream.write(data);
          if (receiverBuffer.size() > 0) {
            handlePacket(receiverBuffer.poll());
          }
        } catch (IOException ex) {
          System.out.println(ex.getMessage());
        }
        break;
    }
  }

  public void closeConnection(){
    super.closeConnection();
    try{
      fileOutputStream.close();
    }catch(IOException ex){
      System.out.println(ex.getMessage());
    }
  }
}
