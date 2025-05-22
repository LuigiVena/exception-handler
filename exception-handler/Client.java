import java.io.*;
import java.net.*;

public class Client {
    public static void main(String[] args) {
        String host = "localhost";
        int port = 8080;

        // Exception throw #1 - Warning
        try {
            // Simulate code that throws
            int x = 10 / 0;

        } catch (Exception ex) {
            // Send the exception type and message to the C server
            try (Socket socket = new Socket(host, port)) {
                OutputStream output = socket.getOutputStream();
                PrintWriter writer = new PrintWriter(output, true);

                String exceptionType = ex.getClass().getSimpleName();
                String exceptionMessage = ex.getMessage();
                String message = String.format("WARNING: %s - %s", exceptionType, exceptionMessage);

                writer.println(message);

            } catch (IOException ioex) {
                ioex.printStackTrace();
            }
        }

        // Exception throw #2 - Error
        try {
            // Simulate code that throws
            int x = 10 / 0;

        } catch (Exception ex) {
            // Send the exception type and message to the C server
            try (Socket socket = new Socket(host, port)) {
                OutputStream output = socket.getOutputStream();
                PrintWriter writer = new PrintWriter(output, true);

                String exceptionType = ex.getClass().getSimpleName();
                String exceptionMessage = ex.getMessage();
                String message = String.format("ERROR: %s - %s", exceptionType, exceptionMessage);

                writer.println(message);

            } catch (IOException ioex) {
                ioex.printStackTrace();
            }
        }

        // Exception throw #3 - Critical
        try {
            // Simulate code that throws
            int x = 10 / 0;

        } catch (Exception ex) {
            // Send the exception type and message to the C server
            try (Socket socket = new Socket(host, port)) {
                OutputStream output = socket.getOutputStream();
                PrintWriter writer = new PrintWriter(output, true);

                String exceptionType = ex.getClass().getSimpleName();
                String exceptionMessage = ex.getMessage();
                String message = String.format("CRITICAL: %s - %s", exceptionType, exceptionMessage);

                writer.println(message);

            } catch (IOException ioex) {
                ioex.printStackTrace();
            }
        }

    }
}
