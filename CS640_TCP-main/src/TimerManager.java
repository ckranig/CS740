import java.util.PriorityQueue;
import java.util.Timer;

// Manages timeout packets by adding and removing stored timeout packets
public class TimerManager {
	PriorityQueue<TimerPacket> packetBuffer = new PriorityQueue<>();
	final Timer timer = new Timer();
	TCPHandler tcpHandler;
	boolean first;
	long to;  //to
	long ertt;    // ERTT 
	long edev;    // EDEV
	

	public TimerManager(TCPHandler tcpHander) {
		this.tcpHandler = tcpHander;
		this.to = (long) 1e9;
	}
	//gettrt
	public long getTO() {
		return to;
	}

	public void updateTO(TCPpacket packet) {
		if (!first) {
			ertt = (System.nanoTime() - packet.timestamp);
			edev = 0;
			to = 2 * ertt;
			first = true;
		} else {
			long srtt = System.nanoTime() - packet.timestamp;
			long sdev = Math.abs(srtt - ertt);
			ertt = 7 * ertt / 8 + 1 * srtt / 8;
			edev = 3 * edev / 4 + 1 * sdev / 4;
			to = ertt + 4 * edev;
		}
	}

	
	// 為packet增加（包裹）計時器
	// 將其加到buffer中
	public void startTimerOnPacket(TCPpacket tcpPacket, int curRetransNum) {
		synchronized (packetBuffer) {
			TimerPacket timerPacket = new TimerPacket(this, tcpPacket, curRetransNum);
			packetBuffer.add(timerPacket);
			timer.schedule(timerPacket, (int) (getTO() / 1000000));
		}
	}

	//三次ACK resend packet
	public void resendPacket(TCPpacket ackPacket) {
		synchronized (packetBuffer) {
			for (TimerPacket thePacket : packetBuffer) {
				if (thePacket.tcpPacket.seqNum == ackPacket.ackNum) {
					resendPacket(thePacket);
					return;
				}
			}
		}
	}

	//重新發送包裹
	public void resendPacket(TimerPacket packet) {
		if (packet.curNumRetrans >= 16) {
			System.out.println("debug: Erorr: max retransmissions reached ... closing connection");
			tcpHandler.endThread();
			return;
		}

		synchronized (packetBuffer) {
			packetBuffer.remove(packet);
			startTimerOnPacket(packet.tcpPacket, packet.curNumRetrans);
			tcpHandler.resendTCPPacket(packet.tcpPacket);
		}
	}

	//把buffer中 < ackNum 的包裹移除： 能收到大於包裹的ackNum 代表小於的包裹已被收到所以才能有更大數字的ackNum
	public void removePacket(int ackNum) {
		synchronized (packetBuffer) {
			while (!packetBuffer.isEmpty() && packetBuffer.peek().tcpPacket.getReturnAck() <= ackNum) {
				TimerPacket timerPacket = packetBuffer.poll();
				timerPacket.cancel();
			}
			timer.purge();
			packetBuffer.notify();
		}
	}

	public void removeAllPacket() {
		synchronized (packetBuffer) {
			timer.cancel();
			packetBuffer.clear();
			packetBuffer.notify();
		}
	}
	//silde window 管控控制 buffer
	public void waitForMoreBufferSpace(int num) {
		synchronized (packetBuffer) {
			try {
				while (packetBuffer.size() > num)
					packetBuffer.wait();
			} catch (InterruptedException ex) {
				Thread.currentThread().interrupt();
			}
		}
	}
}
