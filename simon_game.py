import asyncio
import pygame
import time

from bleak import BleakScanner, BleakClient, BleakError
from contextlib import AsyncExitStack
from functools import partial

# This is th4 Mac side of the simon game

SIMON_NOTIFY_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8"

LVS_HUSH_CTRL_UUID = "5A300002-0023-4BD4-BBD5-A6920E4C5653"
LVS_LUSH_CTRL_UUID = "53300002-0023-4BD4-BBD5-A6920E4C5653"
LVS_DOLCE_CTRL_UUID = "4A300002-0023-4BD4-BBD5-A6920E4C5653"
MOTORBUNNY_CTRL_UUID = "0000fff6-0000-1000-8000-00805f9b34fb"

SHOCKER_CTRL_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8"

evt = asyncio.Event()
shocker_devices = []
shocker_clients = []
simon_devices = []
simon_clients = []
vibe_devices = []
vibe_level = []
vibe_clients = []
vibe_uuid = []

running =  True

vibe_pulse_state = 0
vibe_pulse_time = 0

heading_string = ""
main_string = ""

loop = asyncio.new_event_loop()
asyncio.set_event_loop(loop)

clock=pygame.time.Clock()

def get_lovense_cmd(v):
  cmd = "Vibrate:" + str(v)
  cmd = bytes(cmd, 'utf-8')
  return cmd

def get_motorbunny_cmd(v):
  if v == 0:
    return bytes([0xf0, 0x00, 0x00, 0x00, 0x00, 0xec])
  cmd = [v, 0x14] * 7
  crc = sum(cmd) % 0x100
  cmd += [crc, 0xec]
  cmd = [0xff] + cmd
  return bytes(cmd)

async def write_vibe_level(v, n=0):
  if len(vibe_clients) > n and len(vibe_uuid) > n:
    cmd = None
    if vibe_uuid[n] == MOTORBUNNY_CTRL_UUID:
      cmd = get_motorbunny_cmd(v)
    elif vibe_uuid[n] == LVS_LUSH_CTRL_UUID:
      cmd = get_lovense_cmd(v)
    elif vibe_uuid[n] == LVS_HUSH_CTRL_UUID:
      cmd = get_lovense_cmd(v)
    elif vibe_uuid[n] == LVS_DOLCE_CTRL_UUID:
      cmd = get_lovense_cmd(v)
    else:
      return
    if len(vibe_clients) > n:
      await vibe_clients[n].write_gatt_char(vibe_uuid[n], cmd, False)

async def write_shocker():
    if len(shocker_clients) > 0:
      print("Shock")
      cmd = bytes([0x01, 0x05])
      await shocker_clients[0].write_gatt_char(SHOCKER_CTRL_UUID, cmd, False)

def detection_callback(device, advertisment_data):
  name = str(device.name)
  if name.startswith("Simon") and device not in simon_devices:
    print("Found Simon game " + name)
    simon_devices.append(device)
  if name.startswith("Shocker") and device not in shocker_devices:
    print("Found Shocker device " + name)
    shocker_devices.append(device)
  if name.startswith("MB Controller") and device not in vibe_devices:
    print("Found vibrator " + name)
    vibe_devices.append(device)
    vibe_uuid.append(MOTORBUNNY_CTRL_UUID)
  if name.startswith("LVS-J") and device not in vibe_devices:
    print("Found vibrator " + name)
    vibe_devices.append(device)
    vibe_uuid.append(LVS_DOLCE_CTRL_UUID)

  if len(simon_devices) == 1 and len(vibe_devices) == 1:
    evt.set()

def simon_notify_callback(sender, data):
  print("{sender}: {data}")

def str_to_level(str):
  str = str.decode("utf-8").split(":")
  val = int(str[1]) if len(str) == 2 else 0
  val = val * 10
  val = val if val < 255 else 255
  return val

async def run_buetooth():
    global running
    global main_string
    global heading_string
    global vibe_pulse_state
    global vibe_pulse_time

    print("Starting bluetooth, looking for devices")
    heading_string = "Waiting for connections"
    async with BleakScanner(detection_callback):
      await asyncio.wait_for(evt.wait(), timeout=10.0)

    evt.clear()

    heading_string = ""

    if not running:
      return

    if len(simon_devices) == 0:
      running = False
      return

    async with AsyncExitStack() as stack:
      for d in simon_devices:
        client = BleakClient(d)
        await stack.enter_async_context(client)
        simon_clients.append(client)

        print("Simon connected " + str(client.address))
        #await client.start_notify(SIMON_NOTIFY_UUID, simon_notify_callback)

      for d in shocker_devices:
        client = BleakClient(d)
        await stack.enter_async_context(client)
        shocker_clients.append(client)

        print("Shocker connected " + str(client.address))

      for d in vibe_devices:
        client = BleakClient(d)
        await stack.enter_async_context(client)
        vibe_clients.append(client)
        vibe_level.append(-1)

        print("Vibrator connected " + str(client.address))

      # This is the core game logic
      while running:
          if len(simon_clients) == 1:
            try:
              if time.time() > vibe_pulse_time:
                vibe_pulse_state = (vibe_pulse_state + 5) % 30
                vibe_pulse_time  = time.time() + 0.25

              v = await simon_clients[0].read_gatt_char(SIMON_NOTIFY_UUID)
              v = str_to_level(v)
              v = v + vibe_pulse_state if v > 0 else 0
              if len(vibe_level) > 0 and vibe_level[0] > 0 and v == 0:
                await write_shocker()
              if len(vibe_level) > 0 and vibe_level[0] != v:
                await write_vibe_level(v)
                vibe_level[0] = v
                main_string = "Level " + str(v)
            except BleakError:
              running = False


          await asyncio.sleep(0)

      for c in vibe_clients:
        await write_vibe_level(0)

      main_string = ""

pygame.init()
screen = pygame.display.set_mode((600, 400))
BIG_GAME_FONT = pygame.freetype.SysFont("Courier", 96)
SMALL_GAME_FONT = pygame.freetype.SysFont("Courier", 20)
loop.create_task(run_buetooth())

while running:
  clock.tick(30)

  for event in pygame.event.get():
    if event.type == pygame.QUIT:
      running = False

  # Draw the screen
  screen.fill((255,255,255))
  if len(heading_string) > 0:
    (s, r) = SMALL_GAME_FONT.render(heading_string, (0, 0, 0))
    screen.blit(s, (300 - r.centerx, 50))
  if len(main_string) > 0:
    (s, r) = BIG_GAME_FONT.render(main_string, (0, 0, 0))
    screen.blit(s, (300 - r.centerx, 100))
  pygame.display.flip()

  # Run the async eveny loop once
  loop.call_soon(loop.stop)
  loop.run_forever()

# Run all tasks until they exit
while len(asyncio.all_tasks(loop)):
  loop.call_soon(loop.stop)
  loop.run_forever()

# Shut down the async loop
loop.run_until_complete(loop.shutdown_asyncgens())
loop.close()

pygame.quit()

