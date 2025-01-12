#!/usr/bin/env python3

import motor
import time
import unittest
import sys
import numpy as np

path = None
class TestMotor(unittest.TestCase):
    m = None
    f = None

    @classmethod
    def connect(cls):
        cls.m = motor.MotorManager()
        if (path):
            cls.m.get_motors_by_path([path])
        cls.m.set_auto_count()

    @classmethod
    def setUpClass(cls):
        cls.connect()
        cls.f = open("output.txt", "a+")

    @classmethod
    def tearDownClass(cls):
        cls.m.set_command_mode(motor.ModeDesired.Open)
        cls.m.write_saved_commands()
        cls.f.close()

    def tearDown(self):
        self.m.set_commands([motor.Command()])
        self.m.set_command_mode(motor.ModeDesired.Open)
        self.m.write_saved_commands()
    
    def test_1basics(self):
        self.m.set_command_mode(motor.ModeDesired.Open)
        self.m.write_saved_commands()
        time.sleep(0.001)
        self.assertEqual(self.m.read()[0].host_timestamp_received, 1)

    # sleep is flaky and not important right now
    # def test_2sleep(self):
    #     self.m.set_command_mode(motor.ModeDesired.Sleep)
    #     self.m.write_saved_commands()
    #     time.sleep(1)
    #     try:
    #         tstart = self.m.read()[0].host_timestamp_received
    #         self.m.read() # should timeout on first or second try
    #         self.m.read()
    #         self.assertTrue(False)
    #     except RuntimeError as e:
    #         # todo I don't know how constant this text will be
    #         self.assertTrue(str(e).strip().endswith("Connection timed out"))

    def test_3velocity_mode(self):
        t = 10.0
        v = 5.0
        n = 5.4
        status_start = self.m.read()[0]
        pos_start = status_start.motor_position
        output_start = status_start.joint_position
        self.m.set_command_mode(motor.ModeDesired.Velocity)
        self.m.set_command_velocity([v])
        self.m.write_saved_commands()
        time.sleep(t)
        self.m.set_command_velocity([1])
        self.m.write_saved_commands()
        status_end = self.m.read()[0]
        pos_end = status_end.motor_position
        pos_diff = pos_end-pos_start
        output_diff = status_end.joint_position - output_start
        print("pos diff = " + str(pos_diff))
        print("output_diff = " + str(output_diff*n))
        self.assertTrue(abs(pos_diff - t*v) < .1)
        self.assertTrue(abs(output_diff*n - t*v) < 0.16*n)

    def test_ma732_encoder(self):
        self.m.motors()[0].set_timeout_ms(300)
        mgt = int(self.m.motors()[0]["jmgt"].get(), base=16)
        self.m.motors()[0].set_timeout_ms(10)
        print(f"jmgt = 0x{mgt:04x}")
        self.assertGreater(mgt, 0x101)
        self.assertLessEqual(mgt, 0x707)
        filt = int(self.m.motors()[0]["jfilt"].get())
        print(f"jfilt = {filt}")
        self.assertEqual(filt, 0xbb) # 0xbb is the default value (MA730)
        bct = int(self.m.motors()[0]["jbct"].get()) # check int only
        print(f"jbct = {bct}")
        et = int(self.m.motors()[0]["jet"].get()) # check int only
        print(f"jet = {et}")
        raw = int(self.m.motors()[0]["jraw"].get()) # check int only
        print(f"jraw = {raw}")
        err = int(self.m.motors()[0]["jerr"].get())
        print(f"jerr = {err}")
        self.assertEqual(err, 0)

    def test_current_bandwidth(self):
        self.m.set_command_current_tuning(motor.TuningMode.Chirp, .3, 200, 0)
        self.m.write_saved_commands()
        time_start = time.time()
        freq = np.array([])
        mag = np.array([])
        time.sleep(.1)
        while(time.time() - time_start < 10):
            freq = np.append(freq,[float(self.m.motors()[0]["dft_frequency"].get())])
            mag = np.append(mag,[float(self.m.motors()[0]["dft_magnitude"].get())])


        print(freq)
        mag = 20*np.log10(mag)
        print(mag)
        skip= np.argmax(freq[2:] > 500)+2
        print(skip)
        i = np.argmax(mag[skip:] < -4) # resonance makes 3 not a good choice for motor_enc maxon motor
        bw = freq[skip+i-1]
        print("bandwidth = " + str(bw))
        self.f.write("Benchmarkbandwidth 0 " + str(bw) + " Hz\n")
        self.assertTrue(abs(bw - 1050) < 150)

    def test_logger(self):
        count = 0
        while True:
            str = self.m.motors()[0]["log"].get()
            print(str)
            if str != "log end":
                count += 1
                self.assertLess(count, 100)
            else:
                self.assertGreater(count, 10)
                break

    def test_z_flash_cal(self):
        self.assertEqual(float(self.m.motors()[0]["tgain"].get()), 0.0)
        self.m.motors()[0]["tgain"] = "2.0"
        self.assertEqual(float(self.m.motors()[0]["tgain"].get()), 2.0)
        self.m.motors()[0].set_timeout_ms(3000) # flash cal takes a while
        self.assertEqual(self.m.motors()[0]["flash_cal"].get(), "ok")

        self.m.set_command_mode(motor.ModeDesired.Reset)
        self.m.write_saved_commands()
        time.sleep(5)
        self.connect()
        
        self.assertEqual(float(self.m.motors()[0]["tgain"].get()), 2.0)



if __name__ == "__main__":
    if(len(sys.argv) > 1 and sys.argv[1] == '-p'):
        path = sys.argv[2]
        del(sys.argv[1:3])
    unittest.main()