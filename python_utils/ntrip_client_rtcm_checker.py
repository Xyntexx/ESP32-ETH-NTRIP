from logging import getLogger
from os import getenv
from sys import argv
from time import sleep
from datetime import datetime, timezone, timedelta
from typing import Optional, Dict, Any, Tuple, BinaryIO, List
import logging
import struct
import io
import statistics
import time

from pygnssutils import VERBOSITY_MEDIUM, GNSSNTRIPClient, set_logging
from pyrtcm import RTCMReader, RTCMMessage, RTCMTypeError

from ntrip_config import (
    NTRIP_SERVER,
    NTRIP_PORT,
    NTRIP_USER,
    NTRIP_PASSWORD,
    NTRIP_MOUNTPOINT
)

# Logging configuration
LOG_FILE = "rtcm_log.txt"
VERBOSITY_LEVEL = VERBOSITY_MEDIUM
LOG_FORMAT = "[%(asctime)s] %(message)s"

# RTCM message types
RTCM_MESSAGE_TYPES = {
    "1005": "Stationary RTK Reference Station ARP",
    "1074": "GPS MSM4",
    "1077": "GPS MSM7",
    "1084": "GLONASS MSM4",
    "1087": "GLONASS MSM7",
    "1094": "Galileo MSM4",
    "1097": "Galileo MSM7",
    "1124": "BeiDou MSM4",
    "1127": "BeiDou MSM7",
    "1230": "GLONASS L1 and L2 Code-Phase Biases",
    "1001": "L1 Only GPS RTK Observables",
    "1002": "Extended L1 Only GPS RTK Observables",
    "1003": "L1 and L2 GPS RTK Observables",
    "1004": "Extended L1 and L2 GPS RTK Observables",
    "1019": "GPS Ephemerides",
    "1020": "GLONASS Ephemerides",
    "1033": "Receiver and Antenna Description",
    "1045": "Galileo Ephemerides",
    "1046": "BeiDou Ephemerides",
    "1057": "GPS Compact Ephemerides",
    "1058": "GLONASS Compact Ephemerides",
    "1059": "Galileo Compact Ephemerides",
    "1060": "BeiDou Compact Ephemerides"
}

last_message_received = {}

# GPS time constants
GPS_EPOCH = datetime(1980, 1, 6, tzinfo=timezone.utc)
LEAP_SECONDS = 18  # Current number of leap seconds (as of 2024)

def setup_logging() -> logging.Logger:
    """Configure and return a logger instance."""
    # Create our own logger
    logger = getLogger("ntrip_client_rtcm_checker")
    logger.setLevel(logging.INFO)
    
    # Remove any existing handlers
    logger.handlers = []
    
    # Configure console handler
    console_handler = logging.StreamHandler()
    console_handler.setFormatter(logging.Formatter(LOG_FORMAT))
    logger.addHandler(console_handler)
    
    # Configure file handler
    file_handler = logging.FileHandler(LOG_FILE)
    file_handler.setFormatter(logging.Formatter(LOG_FORMAT))
    logger.addHandler(file_handler)
    
    return logger

def log(msg: str, logger: Optional[logging.Logger] = None) -> None:
    """Log a message to both console and file with timestamp."""
    if logger is None:
        logger = setup_logging()
    logger.info(msg)

class RTCMChecker(io.BufferedWriter):
    """
    RTCM message checker that implements a file-like interface.
    Monitors RTCM message timing and content.
    """
    def __init__(self) -> None:
        """Initialize the RTCM checker."""
        super().__init__(io.BytesIO())
        self.buffer = b""
        self.fastest_gps_time: Optional[datetime] = None
        self._logger = setup_logging()
        self.corrupted_messages = 0
        self.total_messages = 0
        self._stream = io.BytesIO()
        self._reader = RTCMReader(self._stream)
        
        # Interval tracking
        self.interval_deviations: List[float] = []
        self.last_stats_time = time.time()

    def write(self, data: bytes) -> int:
        """Process incoming RTCM data."""
        self.buffer += data
        self._process_buffer()
        
        # Check if it's time to print stats (every minute)
        current_time = time.time()
        if current_time - self.last_stats_time >= 60:
            self._print_stats()
            self.last_stats_time = current_time
            
        return len(data)

    def flush(self) -> None:
        """Flush the buffer."""
        pass

    def close(self) -> None:
        """Close the checker."""
        pass
        
    def _print_stats(self) -> None:
        """Print statistics about message intervals."""
        if not self.interval_deviations:
            self._logger.info("No interval data collected yet.")
            return
            
        max_deviation = max(self.interval_deviations)
        median_deviation = statistics.median(self.interval_deviations)
        
        self._logger.info(
            f"INTERVAL STATS | "
            f"Max distance from 1s: {max_deviation:.4f}s | "
            f"Median distance from 1s: {median_deviation:.4f}s | "
            f"Sample size: {len(self.interval_deviations)}"
        )
        
        # Reset for next minute
        self.interval_deviations = []

    def _process_buffer(self) -> None:
        """Process the current buffer for complete RTCM messages."""
        try:
            # Write buffer to stream and seek to start
            self._stream.write(self.buffer)
            self._stream.seek(0)
            
            # Process messages
            for (raw, parsed) in self._reader:
                if parsed:  # Only process if we got a parsed message
                    self.process_message(parsed, raw)
                    # Update buffer by removing processed data
                    processed_len = len(raw)
                    self.buffer = self.buffer[processed_len:]
                else:
                    self._logger.warning("Received unparsable message")
                    self.corrupted_messages += 1
            
            # Clear stream for next iteration
            self._stream.seek(0)
            self._stream.truncate()
            
        except RTCMTypeError as e:
            self._logger.warning(f"RTCM parsing error: {str(e)}")
            self.corrupted_messages += 1
            # Try to find next sync byte
            sync_index = self.buffer.find(b'\xd3')
            if sync_index > 0:
                self.buffer = self.buffer[sync_index:]
            else:
                self.buffer = b""
        except Exception as e:
            self._logger.warning(f"Error processing buffer: {str(e)}")
            self.buffer = b""

    def process_message(self, msg: RTCMMessage, raw: bytes) -> None:
        """Process a complete RTCM message."""
        msg_type = str(msg.identity)
        msg_desc = RTCM_MESSAGE_TYPES.get(msg_type, "Unknown")
        msg_len = len(raw)

        # Get message time
        time = None
        if msg_type == "1074":
            time = msg.DF004  # GPS TOW
        elif msg_type == "1084":
            time = msg.DF034  # GLONASS TK
        elif msg_type == "1094":
            time = msg.DF248  # Galileo TOW
        elif msg_type == "1124":
            time = msg.DF427  # BeiDou TOW

        # Check message interval
        message_interval = datetime.now() - last_message_received.get(msg_type, datetime.now())
        last_message_received[msg_type] = datetime.now()
        
        # Skip calculating the first interval for each message type (it's not meaningful)
        if msg_type in last_message_received and msg_type != "1230":
            # Calculate the distance from 1 second
            interval_secs = message_interval.total_seconds()
            deviation = abs(interval_secs - 1.0)
            self.interval_deviations.append(deviation)

        # Log if interval is outside expected range (except for type 1230)
        if not timedelta(seconds=0.9) < message_interval < timedelta(seconds=1.1) and msg_type != "1230":
            self._logger.info(
                f"RTCM {msg_type} |"
                f"Interval: {message_interval.total_seconds():.2f} s | "
            )
        
        self.total_messages += 1

def run(
    server: str = NTRIP_SERVER,
    port: int = NTRIP_PORT,
    user: str = NTRIP_USER,
    password: str = NTRIP_PASSWORD,
    mountpoint: str = NTRIP_MOUNTPOINT
) -> None:
    """
    Run the NTRIP client with RTCM message checking.
    
    Args:
        server (str): NTRIP server address
        port (int): Server port
        user (str): Username for authentication
        password (str): Password for authentication
        mountpoint (str): NTRIP mountpoint
    """
    logger = setup_logging()
    checker = RTCMChecker()
    gnc = GNSSNTRIPClient()
    https = 1 if port == 443 else 0

    log("Connecting to NTRIP caster...", logger)

    try:
        gnc.run(
            server=server,
            port=port,
            https=https,
            mountpoint=mountpoint,
            datatype="RTCM",
            ntripuser=user,
            ntrippassword=password,
            ggainterval=-1,
            output=checker,
        )

        while True:
            sleep(3)

    except KeyboardInterrupt:
        log("Terminated by user.", logger)
    except Exception as e:
        log(f"Fatal error: {str(e)}", logger)
        raise

if __name__ == "__main__":
    run() 