import java.net.InetAddress;
import java.net.UnknownHostException;

public class TCPend {
  public static void main(String[] args) {
    int port = 0;
    InetAddress remoteIP = null;
    int remotePort = 0;
    String fileName = null;
    int mtu = 0;
    int sws = 0;
    for (int i = 0; i < args.length; i++) {
      String arg = args[i];
      switch (arg) {
        case "-p": // PORT_FLAG
          port = Integer.parseInt(args[++i]);
          break;
        case "-s": // REMOTE_IP_FLAG
          try {
            remoteIP = InetAddress.getByName(args[++i]);
          } catch (UnknownHostException e) {
            e.printStackTrace();
          }
          break;
        case "-a": // REMOTE_PORT_FLAG
          remotePort = Integer.parseInt(args[++i]);
          break;
        case "-f": // FILE_NAME_FLAG
          fileName = args[++i];
          break;
        case "-m": // MTU_FLAG
          mtu = Integer.parseInt(args[++i]) + 52;
          break;
        case "-c": // SWS_FLAG
          sws = Integer.parseInt(args[++i]);
          break;
        default: // Ignore unknown flags
          break;
      }
    }

    if (port < 0 || remotePort < 0) {
      System.out.println("Debug: port number can not less than 0");
      return;
    }

    if (args.length == 12 && args[2].equals("-s")) {
      Sender hostA = new Sender(port, remoteIP, remotePort, fileName, mtu, sws);
      hostA.start();
      hostA.startFileTransmission();
    } else if (args.length == 8) {
      Receiver hostB = new Receiver(port, mtu, sws, fileName);
      hostB.start();
    } else {
      System.out
          .println("Usage: java TCPend -p <port> -s <remoteIP> -a <remotePort> -f <fileName> -m <mtu> -c <sws>\"");
    }
  }
}
