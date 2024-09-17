
public class WaitClose extends Thread{
  private TCPHandler tcp;
  private long time;

  public WaitClose(TCPHandler tcp, long time){
    this.time = time;
    this.tcp = tcp;
  }
  // 等待time後關閉TCP thread
  public void run() {
    try {
      Thread.sleep(time);
    } catch (InterruptedException ex) {
      Thread.currentThread().interrupt();
    } catch (Exception ex){
      System.out.println(ex.getMessage());
    }
    tcp.endThread();
  }
}
