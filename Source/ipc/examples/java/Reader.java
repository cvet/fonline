import com.mittudev.ipc.Connection;
import com.mittudev.ipc.Message;
import com.mittudev.ipc.ConnectionCallback;
import java.lang.Thread;

class Callback implements ConnectionCallback{
	public int recived = 0;
	public void onMessage(Message msg){
		System.out.println(new String(msg.getData()));
		recived++;
	}
}

public class Reader{
	public static void main(String[] args) {
		Callback cb = new Callback();
		Connection conn = new Connection("ipcdemo", 1);
		conn.setCallback(cb);
		conn.startAutoDispatch();

		while(cb.recived < 3){
			try{
				Thread.sleep(100);
			}catch (Exception e){
				e.printStackTrace();
			}
		}

		conn.stopAutoDispatch();
		conn.close();
		conn.destroy();
	}
}
