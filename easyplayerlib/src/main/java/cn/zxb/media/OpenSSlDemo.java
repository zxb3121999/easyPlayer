package cn.zxb.media;

import android.util.Log;

public class OpenSSlDemo {
    static {
        System.loadLibrary("player-lib");
    }
    private String LOG_T="openssl";
    public native void testOpenssl();
    public native byte[] hashKey(byte[] key, byte[] salt, int count);
    public void ssl(){
        String  str = "xyz";
        byte[] byteKey = str.getBytes();
        byte[] byteSalt = getByteByStr("e258017933f3e629a4166cece78f3162a3b0b7edb2e94c93d76fe6c38198ea12");
        Log.v(LOG_T, "byteSalt: " + getStrByByte(byteSalt));
        Log.v(LOG_T, "byteKey: " + getStrByByte(byteKey));
        byte[] byteArr = hashKey(byteKey, byteSalt, 1);
        Log.v(LOG_T, "tmp1: " + getStrByByte(byteArr));
    }
    byte[] getByteByStr(String str){
        byte[] byteArr = new byte[str.length()/2];
        for(int i = 0; i < str.length(); i += 2){
            String tmp = str.substring(i, i + 2);
            Log.v(LOG_T, "tmp: " + tmp);
            byteArr[i/2] = Integer.valueOf(tmp, 16).byteValue();

            Log.v(LOG_T, "tmp1: " + byteArr[i/2]);
        }
        return byteArr;
    }
    String getStrByByte(byte[] byteArr){
        String result = "";
        for(int i = 0; i < byteArr.length; i++){
            String hex = Integer.toHexString(byteArr[i] & 0xFF);
            if (hex.length() == 1) {
                hex = '0' + hex;
            }
            result += hex.toUpperCase();
        }
        return result;
    }
}
