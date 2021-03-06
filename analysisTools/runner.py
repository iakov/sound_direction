import os
import subprocess

class Runner(object):

    PARAM_WINDOW_SIZE = ['--window-size']
    PARAM_HISTORY_DEPTH = ['--history-depth']
    PARAM_INPUT_FN = ['-f', '--filename']

    __args = []
    __dict_args = {}
    __out_angle = []
    __out_vad   = []
    __env = {}

    def __init__(self, args):

        self.__args = args

        tn_param = next(param for param in self.__args if (param in self.PARAM_INPUT_FN))
        tn_ind   = self.__args.index(tn_param)
        self.__dict_args['input_filename'] = self.__args[tn_ind + 1]

        ws_param    = next(param for param in self.__args if (param in self.PARAM_WINDOW_SIZE))
        ws_ind      = self.__args.index(ws_param)
        self.__dict_args['window_size'] = int(self.__args[ws_ind + 1])

        hd_param      = next(param for param in self.__args if (param in self.PARAM_HISTORY_DEPTH))
        hd_ind        = self.__args.index(hd_param)
        self.__dict_args['history_depth'] = int(self.__args[hd_ind + 1])

        self.__env = dict(os.environ)
        self.__env['LD_LIBRARY_PATH'] = '../bin/desktop-debug/'

    def run(self):

        try:
            out = subprocess.check_output(self.__args, stderr=subprocess.STDOUT, env=self.__env)
            out = out.split('\n')
            self.__out_angle = []
            self.__out_vad = []
            for line in out:
                if not line:
                    continue
                data = line.split()
                self.__out_angle.append(int(data[0]))
                self.__out_vad.append((float(data[1])))
            return 0
        except subprocess.CalledProcessError as exc:
            return exc.returncode

    def get_args(self):

        return self.__dict_args

    def get_angle_out(self):

        return self.__out_angle

    def get_vad_out(self):

        return self.__out_vad
