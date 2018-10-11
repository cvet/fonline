import com.mittudev.ipc.Connection;
import com.mittudev.ipc.Message;
import com.mittudev.ipc.ConnectionCallback;
import java.lang.Thread;

public class Writer {
	public static void main(String[] args) {
		Connection conn = Connection.connect("ipcdemo", 1);

		System.out.println("Sent: message1");
		Message msg = new Message("Message 1".getBytes());
		conn.send(msg);
		msg.destroy();

		System.out.println("Sent: message2");
		msg = new Message("Message 2".getBytes());
		conn.send(msg);
		msg.destroy();

		System.out.println("Sent: message3");
		msg = new Message("Message 3".getBytes());
		conn.send(msg);
		msg.destroy();

		conn.destroy();
	}
}
