// ==================== Serializers.java ====================
import java.io.*;

/**
 * Serializers for MPI-like communication
 * Handles conversion between Java types and network byte streams
 */
public class Serializers {

    /**
     * Interface for type serialization/deserialization
     */
    public interface Serializer<T> {
        void send(T value, DataOutputStream dos) throws IOException;
        T receive(DataInputStream dis) throws IOException;
    }

    /**
     * Integer serializer
     */
    public static final Serializer<Integer> INTEGER = new Serializer<Integer>() {
        @Override
        public void send(Integer value, DataOutputStream dos) throws IOException {
            dos.writeInt(value);
            dos.flush();
        }

        @Override
        public Integer receive(DataInputStream dis) throws IOException {
            return dis.readInt();
        }
    };

    /**
     * Double serializer
     */
    public static final Serializer<Double> DOUBLE = new Serializer<Double>() {
        @Override
        public void send(Double value, DataOutputStream dos) throws IOException {
            dos.writeDouble(value);
            dos.flush();
        }

        @Override
        public Double receive(DataInputStream dis) throws IOException {
            return dis.readDouble();
        }
    };

    /**
     * Long serializer
     */
    public static final Serializer<Long> LONG = new Serializer<Long>() {
        @Override
        public void send(Long value, DataOutputStream dos) throws IOException {
            dos.writeLong(value);
            dos.flush();
        }

        @Override
        public Long receive(DataInputStream dis) throws IOException {
            return dis.readLong();
        }
    };

    /**
     * Boolean serializer
     */
    public static final Serializer<Boolean> BOOLEAN = new Serializer<Boolean>() {
        @Override
        public void send(Boolean value, DataOutputStream dos) throws IOException {
            dos.writeInt(value ? 1 : 0);
            dos.flush();
        }

        @Override
        public Boolean receive(DataInputStream dis) throws IOException {
            return dis.readInt() == 1;
        }
    };

    /**
     * String serializer (UTF-8)
     */
    public static final Serializer<String> STRING = new Serializer<String>() {
        @Override
        public void send(String value, DataOutputStream dos) throws IOException {
            byte[] bytes = value.getBytes("UTF-8");
            dos.writeInt(bytes.length);
            dos.write(bytes);
            dos.flush();
        }

        @Override
        public String receive(DataInputStream dis) throws IOException {
            int length = dis.readInt();
            byte[] bytes = new byte[length];
            dis.readFully(bytes);
            return new String(bytes, "UTF-8");
        }
    };

    /**
     * Float serializer
     */
    public static final Serializer<Float> FLOAT = new Serializer<Float>() {
        @Override
        public void send(Float value, DataOutputStream dos) throws IOException {
            dos.writeFloat(value);
            dos.flush();
        }

        @Override
        public Float receive(DataInputStream dis) throws IOException {
            return dis.readFloat();
        }
    };

    // Private constructor to prevent instantiation
    private Serializers() {}
}