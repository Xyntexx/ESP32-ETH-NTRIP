import socket
import sys
import time

def main():
    # Create UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    # Allow socket to be reused
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    # Bind to all interfaces on port 8888
    server_address = ('0.0.0.0', 8888)
    print(f'Starting UDP receiver on {server_address[0]}:{server_address[1]}')
    sock.bind(server_address)
    
    # Buffer to store incoming data
    message_buffer = bytearray()
    last_receive_time = time.time()
    
    try:
        while True:
            # Set timeout to 0.1 seconds
            sock.settimeout(0.1)
            
            try:
                # Receive data
                data, address = sock.recvfrom(4096)
                message_buffer.extend(data)
                last_receive_time = time.time()
                
            except socket.timeout:
                # If we have buffered data and no new data for 0.1 seconds, process the message
                if message_buffer and (time.time() - last_receive_time) > 0.1:
                    # Print the complete message
                    try:
                        # Try to decode as UTF-8 text
                        text = message_buffer.decode('utf-8')
                        print(f'[{address[0]}] {text}')
                    except UnicodeDecodeError:
                        # If not UTF-8, print as hex
                        print(f'[{address[0]}] Hex: {message_buffer.hex()}')
                    
                    # Clear the buffer
                    message_buffer.clear()
            
    except KeyboardInterrupt:
        print('\nStopping UDP receiver...')
    finally:
        sock.close()

if __name__ == '__main__':
    main() 