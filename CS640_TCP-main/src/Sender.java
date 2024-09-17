import java.net.InetAddress;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.lang.Math;

// Sends file to receiver using TCP from base class
public class Sender extends TCPHandler {
  private static final int HEADER_SIZE = 52;
  public Sender(int port, InetAddress ip, int remotePort, String fileName, int mtu, int sws){
    super(port, fileName, mtu, sws);
    this.remoteIP = ip;
    this.remotePort = remotePort;
  }

  //wait for establishing connection
  private void establishConnection() {
    while (!established);
  }

  public void startFileTransmission(){
    establishConnection();
    if(!running())
      return;

    FileInputStream file_stream = null;
    
    try{
      file_stream = new FileInputStream(fileName);
    }catch(FileNotFoundException ex){
      System.out.println(ex.getMessage());
    }

    try{
      while(true){
        assert file_stream != null;
        if (!(file_stream.available() > 0))
          break;
        timerManager.waitForMoreBufferSpace(sws-1);
        byte[] data = new byte [Math.min(this.mtu - HEADER_SIZE, file_stream.available())];
        int numread = file_stream.read(data, 0, data.length);
        if(!running())
          return;
        dataTransferred += numread;
        Boolean[] tcpFlags = {false, false, false};
        sendTCP(data, tcpFlags);
      }

      file_stream.close();
    }catch(IOException ex){
      System.out.println(ex.getMessage());
    }

    timerManager.waitForMoreBufferSpace(0);
    if(!running())
      return;

    initiatedClose = true;
    //sendFIN 送出第一包ＦＩＮ
    this.sendTCP(new byte[0], new Boolean[] { false, true, false }); // { - F - }
  }

  public void handlePacket(TCPpacket packet){

  }
}
