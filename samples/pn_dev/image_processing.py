import logging
import multiprocessing as mp
import queue
import random
from enum import IntEnum, Enum

# this is only needed to type hint objects created using the
# multiprocessing.Event factory function
from multiprocessing.synchronize import Event as MpEventType

# setup logging
logging.basicConfig(format='Python %(levelname)s: %(message)s', level=logging.DEBUG)

# ---------------------------------------------------------------------
# Types
# ---------------------------------------------------------------------


class Command(IntEnum):
    """Commands as defined in interface.h

    `IntEnum` is used instead of `Enum` so the enums can be compared to ints.
    """

    NOP = 0x00
    REBOOT = 0x01
    PING = 0x02
    SET_WORKPIECE_TYPE_NONE = 0x03
    SET_WORKPIECE_TYPE_122 = 0x04  # Prefix for plowsteel article numbers
    TAKE_PICTURE = 0x10
    SET_WORKPIECE_ORIENTATION = 0x11
    SET_WORKPIECE_SERIAL_NUMBER = 0x12


class Status(Enum):
    """Statuses as defined in interface.h"""

    UNDEFINED = 0x00
    BOOTING = 0x01
    PING_REPLY = 0x02
    BUSY = 0x03
    ERROR = 0x04
    WORKPIECE_OK = 0x05
    WORKPIECE_NOK = 0x06
    WORKPIECE_NONE = 0x07

    # TODO-bwahl - do we need this? this wasn't defined in the original interface
    COMMAND_ACK = 0x08


class Error(Enum):
    """Errors as defined in interface.h"""

    UNDEFINED = 0x00
    INVALID_COMMAND = 0x03
    INVALID_PARAMETER = 0x04
    NO_CAMERA = 0x05
    INTERNAL = 0x06
    INVALID_WORKPIECE_SIZE = 0x07
    INVALID_WORKPIECE_COLOR = 0x08
    INVALID_WORKPIECE_SHAPE = 0x09
    INVALID_WORKPIECE_WEIGHT = 0x0A
    INVALID_WORKPIECE_TEXTURE = 0x0B


class StatusQueueItem:
    def __init__(
        self,
        error: Error = Error.UNDEFINED,
        status: Status = Status.UNDEFINED,
    ):
        self.error = error
        self.status = status

    def as_int_tuple(self) -> tuple[int, int]:
        return self.error.value, self.status.value

    def __repr__(self) -> str:
        return f"{self.error}, {self.status}"


class CommandQueueItem:
    def __init__(self, cmd: Command = Command.NOP, param: int = 0) -> None:
        self.cmd = cmd
        self.param = param


# ---------------------------------------------------------------------
# Variables
# ---------------------------------------------------------------------

stop_event = mp.Event()
command_queue: "mp.Queue[CommandQueueItem]" = mp.Queue(maxsize=1)
status_queue: "mp.Queue[StatusQueueItem]" = mp.Queue(maxsize=1)

# ---------------------------------------------------------------------
# Functions
# ---------------------------------------------------------------------


def image_processing_main(
    command_queue: "mp.Queue[CommandQueueItem]",
    status_queue: "mp.Queue[StatusQueueItem]",
    stop_event: MpEventType,
) -> None:
    """Main entry point into image processing. This runs in its own process and
    does not share memory with the main app process.

    Args:
        command_queue (mp.Queue[CommandQueueItem]): Queue of commands to read from
        status_queue (mp.Queue[StatusQueueItem]): Queue of statuses to write to
        stop_event (Event): Event that signals when this process should stop
    """
    # If a command is present on the command queue, status_queue_item is
    # updated and put onto the status queue during calls to process_command
    status_queue_item = StatusQueueItem()

    workpiece_type = 0

    def take_picture() -> None:
        # simulate getting and using workpiece type
        logging.debug(f"img loop: workpiece_type={workpiece_type}")

        # generate a no-camera error 10% of the time
        cam_present = random.randint(1, 10) != 1
        if not cam_present:
            logging.debug("img loop: no camera present!")
            status_queue_item.status = Status.ERROR
            status_queue_item.error = Error.NO_CAMERA
            return

        # force it to be 33% pass, 67% fail. this is to help develop retry
        # logic.
        img_pass = random.randint(1, 3) == 1
        logging.debug(f"img loop: img_pass={img_pass}")
        status_queue_item.error = Error.UNDEFINED
        status_queue_item.status = (
            Status.WORKPIECE_OK if img_pass else Status.WORKPIECE_NOK
        )

    def set_workpiece_type(wp_type: int) -> None:
        logging.debug("img loop: updating workpiece type")
        nonlocal workpiece_type
        workpiece_type = wp_type
        status_queue_item.status = Status.COMMAND_ACK
        status_queue_item.error = Error.UNDEFINED

    def process_command() -> None:
        """Check to see if a command is on the command queue. If there is,
        process it and put a status on the status queue.
        """
        try:
            # can't have a blocking wait here or we won't be able to handle
            # stop_event and we'll deadlock on the join
            cmd_item = command_queue.get_nowait()
        except queue.Empty:
            return

        match cmd_item.cmd:
            case Command.SET_WORKPIECE_TYPE_122:
                set_workpiece_type(122)

            case Command.TAKE_PICTURE:
                take_picture()

            case _:
                status_queue_item.status = Status.ERROR
                status_queue_item.error = Error.INVALID_COMMAND

        # put the status on the status queue
        try:
            status_queue.put(status_queue_item)
            logging.debug(f"img loop: put successful, status_queue_item={status_queue_item}")
        except queue.Full:
            logging.debug(f"img loop: queue full")
            pass

    # main image processing loop
    while not stop_event.is_set():
        process_command()

        # other processing can run here

    logging.debug("img loop: finished!")


def init() -> None:
    """Initialize this module.

    Responsible for starting the image processing process.
    """
    # we need to keep a reference to the process so we can join it later
    stop_event.clear()
    global process
    process = mp.Process(
        target=image_processing_main,
        args=(command_queue, status_queue, stop_event),
    )
    process.start()


def deinit() -> None:
    """Deinitialize this module.

    Responsible for stopping and joining the image processing process.
    """
    logging.debug("setting close event")
    stop_event.set()
    process.join()


def process_command(cmd: int, param: int) -> tuple[int, int]:
    """Process a command received from the PLC. Forward the command to the
    image processing process and block until a response is received. This runs
    in the main app process/thread.

    Args:
        cmd (int): The command received from the PLC
        param (int): The parameter associated with the command

    Returns:
        tuple[int, int]: A two-element tuple of ints containing the error code
        and status code, respectively.
    """
    logging.debug(f"process_command: cmd={Command(cmd)}, param={param}")

    status_queue_item = StatusQueueItem()
    if cmd in list(Command):
        command_queue.put(CommandQueueItem(Command(cmd), param))
        status_queue_item = status_queue.get()
    else:
        status_queue_item.status = Status.ERROR
        status_queue_item.error = Error.INVALID_COMMAND

    logging.debug(f"process_command: returning {status_queue_item}")

    return status_queue_item.as_int_tuple()
