import java.util.TimerTask;

//為每一個packet製造timer(per thread)
public class TimerPacket extends TimerTask implements Comparable<TimerPacket>{

	public int curNumRetrans;
	public TCPpacket tcpPacket;
	private TimerManager timerManager;

	public TimerPacket(TimerManager timemanager, TCPpacket tcpPacket, int curNumRetrans){
		this.timerManager = timemanager;
		this.tcpPacket = tcpPacket;
		this.curNumRetrans = curNumRetrans;
	}

	// Thread for timer packet
	public void run(){
		curNumRetrans += 1;
		timerManager.resendPacket(this);
	}

	@Override
	public int compareTo(TimerPacket other) {
		return this.tcpPacket.seqNum - other.tcpPacket.seqNum;
	}
}
