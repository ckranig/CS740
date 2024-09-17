import java.nio.ByteBuffer;

public class TCPpacket {
  // header part
  public int seqNum;
  public int ackNum;
  public long timestamp;
  public Boolean[] flags = new Boolean[3]; // {S F A}
  short checksum;
  public static final int headerSize = 24; // 4 * int(4 byte) + 1 * long (8 byte) = 24 byte
  // payload part
  public byte[] data;

  public TCPpacket(int seqNum, int ackNum, long time, Boolean[] flags, byte[] data) {
    this.seqNum = seqNum;
    this.ackNum = ackNum;
    this.timestamp = time;
    this.flags = flags;
    this.data = data;
    this.checksum = 0;
  }

  public TCPpacket() {
  }

  public boolean verifyChecksum(byte[] data) {
    this.deserialize(data);// 透過deserialize會將data的資訊填到此TCPpacket裡
    short oldCheckSum = this.checksum;
    this.checksum = 0;
    this.serialize(); // 透過serialize計算新的checksum
    return oldCheckSum == this.checksum;
  }

  public boolean getSyn() { // {S}
    return flags[0];
  }

  public boolean getFin() { // {F}
    return flags[1];
  }

  public boolean getAck() { // {A}
    return flags[2];
  }

  public boolean isDataPacket() { // {D}
    return data.length > 0 ? true : false;
  }

  public int getReturnAck() {
    return this.seqNum + this.data.length;
  }

  // todo:序列化
  public byte[] serialize() {
    // for none data packet
    int dataLength = 0;
    // for data packet
    if (data != null) {
      dataLength = this.data.length;
    }
    int length = headerSize + dataLength;

    byte[] serializedPacket = new byte[length];
    ByteBuffer bytebuffer = ByteBuffer.wrap(serializedPacket);

    bytebuffer.putInt(this.seqNum);
    bytebuffer.putInt(this.ackNum);
    bytebuffer.putLong(this.timestamp);

    int lengthWithFlags = dataLength;
    // {S0 F1 A2}
    int i = 0;
    while (i < 3) {
      lengthWithFlags = lengthWithFlags << 1;
      lengthWithFlags += flags[i] ? 1 : 0; // 把flags加到最右邊 {length/S/F/A}
      i++;
    }
    bytebuffer.putInt(lengthWithFlags);
    bytebuffer.putShort((short) 0); // allZero
    // checksum先填0
    this.checksum = 0;
    bytebuffer.putShort(this.checksum);
    // data
    if (data != null) {
      bytebuffer.put(this.data);
    }
    // 計算checksum
    if (this.checksum == 0) {
      bytebuffer.rewind();
      int CS = calculateChecksum(bytebuffer.array());
      this.checksum = (short)CS;
      bytebuffer.putShort(22, this.checksum);
    }
    return serializedPacket;
  }

  // todo:反序列化 : 將serializedPacket的資訊填到本class中
  public TCPpacket deserialize(byte[] serializedPacket) {
    ByteBuffer byteBuffer = ByteBuffer.wrap(serializedPacket);
    // seqNum
    this.seqNum = byteBuffer.getInt();
    // ackNum
    this.ackNum = byteBuffer.getInt();
    // timestamp
    this.timestamp = byteBuffer.getLong();
    int lengthWithFlags = byteBuffer.getInt();
    // length
    int dataLength = lengthWithFlags >> 3;
    if (dataLength > serializedPacket.length - headerSize) {
      System.out.println("debug: dataLength too long");
      return null;
    }
    // flags
    int i = 0;
    while (i < 3) {
      flags[2 - i] = lengthWithFlags % 2 == 1; // {length/S/F/A}
      lengthWithFlags = lengthWithFlags >> 1;
      i++;
    }
    // allZero
    byteBuffer.getShort();
    // checksum
    this.checksum = byteBuffer.getShort();
    // data
    this.data = new byte[dataLength];
    byteBuffer.get(data);
    return this;
  }

  // done: checksum
  public static int calculateChecksum(byte[] data) {
    // 1. 准备数据
    int length = data.length;
    // 2. 计算校验和
    int sum = 0;
    // 将数据按16位分割并求和
    for (int i = 0; i < length - 1; i += 2) {
      int segment = (data[i] & 0xFF) << 8 | (data[i + 1] & 0xFF);
      sum += segment;
      // 处理进位
      if ((sum & 0xFFFF0000) != 0) {
        sum = (sum & 0xFFFF) + 1;
      }
    }
    // 如果数据长度为奇数，最后一个字节单独处理
    if (length % 2 != 0) {
      int lastSegment = (data[length - 1] & 0xFF) << 8;
      sum += lastSegment;
      if ((sum & 0xFFFF0000) != 0) {
        sum = (sum & 0xFFFF) + 1;
      }
    }
    // 3. 求反码
    int checksum = ~sum & 0xFFFF;
    return checksum;
  }
}
