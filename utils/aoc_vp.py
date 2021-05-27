#!/usr/bin/python

import argparse
import sys
import re
import json

#
# This file is part of OpenATS COMPASS.
#
# COMPASS is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# COMPASS is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with COMPASS. If not, see <http://www.gnu.org/licenses/>.
#

def hms2sec(h,m,s,ms):
    return (h*60+m)*60+s+ms/1000

def time_delta(st, et):
    [sh, sm, ss, sms] = list(map(lambda x: int(x), re.split(':|\.', st)))
    [eh, em, es, ems] = list(map(lambda x: int(x), re.split(':|\.', et)))
    return float('{0:.3f}'.format(hms2sec(eh,em,es,ems) - hms2sec(sh,sm,ss,sms)))

class context:
    def __init__(self):
        self.__context = 'init'

    def change(self, state):
        self.__context = state

    def __eq__(self, other):
        return self.__context == other

# parsing tokens

# general
aoc_version = re.compile(r'^AOC\s+Version\s+V(\S+)$')
time_now = re.compile(r'^NOW\s+(\S+ \S+)\s+UTC$')
filename = re.compile(r'"(\S+)"')
delta_time = re.compile(r'^DT\s+(\S+)\s+secs\s+\(STD\s+(\S+)\s+secs\)$')
sac_sic = re.compile(r'0x([\da-f]{2})([\da-f]{2})')
time_start_end = re.compile(r'(\d{2}):(\d{2}):(\d{2})\.(\d{3})\s+UTC\s+\.\.\.\s+(\d{2}):(\d{2}):(\d{2})\.(\d{3})\s+UTC')
time_duration = re.compile(r'\s+(\d{2}):(\d{2}):(\d{2})\.(\d{3})\s+')

# 'out' file deviations
short_track = re.compile(r'Short (\w+) track #(\d+) \((\w+) track #(\d+) \[A:([^]]+)\] \[S:([^]]+)\]\) starting near (\S+) UTC \(diff=([\d.]+)\s+seconds\)$')

# 'hlp' file deviations
extra_track = re.compile(r'Extra test track #(\d+)\.(\d+)\s+Time of [\w ]+: (\S+) UTC\s+Time of [\w ]+: (\S+) UTC\s+')
missing_track = re.compile(r'Missing reference track #(\d+)\.(\d+)\s+Time of [\w ]+: (\S+) UTC\s+Time of [\w ]+: (\S+) UTC\s+')
get_track_candidates = re.compile(r'searching \.\.\..*?(\S+)\s+candidate[s]?\s+found')
get_trkNr = re.compile(r'#(\d+)\.')
get_modeA = re.compile(r'SSR mode 3/A code; value=0(\d+)')
get_modeS = re.compile(r'aircraft address; value=0x(\S+)')
get_modeC = re.compile(r'SSR mode C code; value=(\S+)')

deviation = re.compile(r'^DEV\s+#(\d+):\s+(\S+)\s+(\S+)')
track_deviation = re.compile(r'Test track #(\d+)\.(\d+) \(reference track #(\d+)\.(\d+)\) near (\d{2}):(\d{2}):(\d{2})\.(\d{3}) UTC:\s+(\w+)\s+')
track_reported = re.compile(r'\s+reported ([\w.]+)=(.+); expected (.+)\s+REF')
track_unexpected = re.compile(r'\s+unexpected (.+)\s+REF')
get_times = re.compile(r'\[(\S+)\]')

class aoc_parser:
    def __init__(self, parms, dct):
        self.__dct = dct
        self.__parms = parms
        self.__id = 0
        with open(parms['hlp'], encoding="UTF-8") as hlp_file:
            self.__deviation = ''
            for line in hlp_file:
                token = line[0:3]
                line = line.rstrip('\n')
                if token in aoc_tokens_hlp:
                    aoc_tokens_hlp[token](self, line)
                elif line != '' and self.__deviation != '':
                    self.__deviation += line
                elif line == '': # empty line => handle found deviation
                    if self.__deviation:
                        self.handle_deviation()
        with open(parms['out'], encoding="UTF-8") as out_file:
            self.__context = context()
            for line in out_file:
                for token in aoc_tokens_out.keys():
                    if re.search(token, line):
                        if isinstance(aoc_tokens_out[token],str):
                            self.__context.change(aoc_tokens_out[token])
                        else:
                            aoc_tokens_out[token](self, line)

    def __repr__(self):
        return str(self.__dct)

    def viewpoints(self):
        return self.__dct

    def time_now(self, line):
        time = time_now.findall(line)
        if time:
            self.__dct['view_point_context'].update({"time_now" : time[0]})

    def aoc_version(self, line):
        version = aoc_version.findall(line)
        if version:
            self.__dct['view_point_context'].update({"aoc_version" : version[0]})

    def fname_ref(self, line):
        fname = filename.findall(line)
        if fname:
            self.__dct['view_point_context']['datasets'][0].update({'filename':fname[0]})

    def fname_tst(self, line):
        fname = filename.findall(line)
        if fname:
            self.__dct['view_point_context']['datasets'][1].update({'filename':fname[0]})

    def delta_time(self, line):
        res = delta_time.findall(line)
        if res:
            [to, to_std] = list(map(lambda x: float(x), list(res)[0]))
            self.__dct['view_point_context']['datasets'][1].update({"time_offset" : to, "time_offset_stddev" : to_std})

    def sac_sic(self, line):
        res = sac_sic.findall(line)
        if res:
            [sac, sic] = list(map(lambda x: int(x,16), list(res)[0]))
            if self.__context == 'ref':
                self.__dct['view_point_context']['datasets'][0].update({'ds_sac':sac,'ds_sic':sic})
            elif self.__context == 'tst':
                self.__dct['view_point_context']['datasets'][1].update({'ds_sac':sac,'ds_sic':sic})
                # check if an override SAC:SIC is required
                if sac == self.__dct['view_point_context']['datasets'][0]['ds_sac'] and sic == self.__dct['view_point_context']['datasets'][0]['ds_sic']:
                    self.__dct['view_point_context']['datasets'][1].update({'ds_sac_override':sac,'ds_sic_override':(sic+1) % 256})

    def time(self, line):
        res = time_start_end.findall(line)
        if res:
            [sh,sm,ss,sms,eh,em,es,ems] = list(map(lambda x: int(x), list(res)[0]))
            start = hms2sec(sh,sm,ss,sms)
            end = hms2sec(eh,em,es,ems)
            if self.__context == 'ref':
                self.__dct['view_point_context']['datasets'][0].update({'time_start':start,'time_end':end})
            elif self.__context == 'tst':
                self.__dct['view_point_context']['datasets'][1].update({'time_start':start,'time_end':end})
                # calculate common_time_duration
                refS = self.__dct['view_point_context']['datasets'][0]['time_start']
                refE = self.__dct['view_point_context']['datasets'][0]['time_end']
                if refS < end and start < refE:
                    self.__dct['view_point_context'].update({'common_time_duration':float('{0:.3f}'.format(min(refE,end) - max(refS,start)))})
                else:
                    del(self.__dct['view_point_context']['common_time_duration'])

    def duration(self, line):
        res = time_duration.findall(line)
        if res:
            [h,m,s,ms] = list(map(lambda x: int(x), list(res)[0]))
            duration = hms2sec(h,m,s,ms)
            if self.__context == 'ref':
                self.__dct['view_point_context']['datasets'][0].update({'duration':duration})
            elif self.__context == 'tst':
                self.__dct['view_point_context']['datasets'][1].update({'duration':duration})

    def start_deviation(self, line):
        self.__devpar = {}
        res = deviation.findall(line)
        if res:
            self.__devpar = {
                'id' : self.__id,
                'rn' : self.__parms['rn'],
                'tn' : self.__parms['tn'],
                'nr' : res[0][0],
                'type' : res[0][1],
                'qual' : res[0][2],
            }
            self.__id += 1
        self.__deviation = line

    def handle_deviation(self):
        key = ''
        if 'type' in self.__devpar and 'qual' in self.__devpar:
            key = self.__devpar['type'] + ' ' + self.__devpar['qual']
        if key != '' and key in aoc_tokens_deviation:
            aoc_tokens_deviation[key](self)
        elif key != '':
            print('handle_deviation(missing definition) :: parms:',self.__devpar,'deviation:',self.__deviation)
        elif not re.match('Deviations by type', self.__deviation):
            print('handle_deviation(unexpected definition) :: deviation:',self.__deviation)
        self.__devpar = {}
        self.__deviation = ''

    def extra_track(self):
        template = {
            "id": self.__devpar['id'],
            "type": "Extra Track",
            "name": "DEV #" + self.__devpar['nr'],
            "text": None,
            "db_objects": [
                "Tracker"
            ],
            "filters": {
                "Time of Day": {
                    "Time of Day Minimum": None,
                    "Time of Day Maximum": None
                },
                "Tracker Track Number": {
                }
            },
            "deviations": [
                "{}".format(self.__devpar['nr'])
            ]
        }

        extra = extra_track.findall(self.__deviation)
        if extra:
            template['filters']['Time of Day'].update({'Time of Day Minimum':extra[0][2]})
            template['filters']['Time of Day'].update({'Time of Day Maximum':extra[0][3]})
            template['filters']['Tracker Track Number'].update({self.__devpar['tn'] + ' track_num':extra[0][0]})

            context = ''
            candidates = get_track_candidates.findall(self.__deviation)
            if candidates:
                if candidates[0] != 'no':
                    tn = get_trkNr.findall(self.__deviation)
                    cand_tn = ','.join(sorted(set(tn[1:])))
                    template['filters']['Tracker Track Number'].update({self.__devpar['rn'] + ' track_num':cand_tn})
                context = '{} candidates'.format(candidates[0])
            modeA = get_modeA.findall(self.__deviation)
            if modeA:
                context += '; mode 3/A:{}'.format(modeA[0])
            modeS = get_modeS.findall(self.__deviation)
            if modeS:
                context += '; aircraft address:0x{}'.format(modeS[0])
            modeC = get_modeC.findall(self.__deviation)
            if modeC:
                context += '; mode C:{}'.format(modeC[0])

            if candidates[0] == '1':
                template.update({'type':'Extra Track dubious'})
            elif candidates[0] != 'no':
                template.update({'type':'Extra Track dubious multiple'})
                if modeA:
                   template['filters'].update({'Mode 3/A Codes':{'Mode 3/A Codes Values':modeA[0]}})
                if modeS:
                   template['filters'].update({'Target Address':{'Target Address Values':modeS[0]}})

            template.update({'text':self.__devpar['tn'] + ' {}:({}.{})\n{}'.format(extra[0][0],extra[0][0],extra[0][1],context)})

        self.__dct['view_points'].append(template)

    def missing_track(self):
        template = {
            "id": self.__devpar['id'],
            "type": "Missing Track",
            "name": "DEV #" + self.__devpar['nr'],
            "text": None,
            "db_objects": [
                "Tracker"
            ],
            "filters": {
                "Time of Day": {
                    "Time of Day Minimum": None,
                    "Time of Day Maximum": None
                },
                "Tracker Track Number": {
                }
            },
            "deviations": [
                "{}".format(self.__devpar['nr'])
            ]
        }

        missing = missing_track.findall(self.__deviation)
        if missing:
            template['filters']['Time of Day'].update({'Time of Day Minimum':missing[0][2]})
            template['filters']['Time of Day'].update({'Time of Day Maximum':missing[0][3]})
            template['filters']['Tracker Track Number'].update({self.__devpar['rn'] + ' track_num':missing[0][0]})

            context = ''
            candidates = get_track_candidates.findall(self.__deviation)
            if candidates:
                if candidates[0] != 'no':
                    tn = get_trkNr.findall(self.__deviation)
                    cand_tn = ','.join(sorted(set(tn[1:])))
                    template['filters']['Tracker Track Number'].update({self.__devpar['tn'] + ' track_num':cand_tn})
                context = '{} candidates'.format(candidates[0])
            modeA = get_modeA.findall(self.__deviation)
            if modeA:
                context += '; mode 3/A:{}'.format(modeA[0])
            modeS = get_modeS.findall(self.__deviation)
            if modeS:
                context += '; aircraft address:0x{}'.format(modeS[0])
            modeC = get_modeC.findall(self.__deviation)
            if modeC:
                context += '; mode C:{}'.format(modeC[0])

            if candidates[0] == '1':
                template.update({'type':'Missing Track dubious'})
            elif candidates[0] != 'no':
                template.update({'type':'Missing Track dubious multiple'})
                if modeA:
                   template['filters'].update({'Mode 3/A Codes':{'Mode 3/A Codes Values':modeA[0]}})
                if modeS:
                   template['filters'].update({'Target Address':{'Target Address Values':modeS[0]}})

            template.update({'text':self.__devpar['rn'] + ' {}:({}.{})\n{}'.format(missing[0][0],missing[0][0],missing[0][1],context)})

        self.__dct['view_points'].append(template)

    def short_track(self, line):
        template = {
            "id": self.__id,
            "type": "Short Track",
            "name": "DEV #{}".format(self.__id + 1),
            "text": None,
            "time": None,
            "time_window": None,
            "db_objects": [
                "Tracker"
            ],
            "filters": {
                # "Time of Day": {
                #     "Time of Day Minimum": None,
                #     "Time of Day Maximum": None
                # },
                "Tracker Track Number": {
                }
            },
            "deviations": [
                "{}".format(self.__id + 1)
            ]
        }

        res = short_track.findall(line)
        if res:
            t1 = self.__parms['rn']
            t2 = self.__parms['tn']
            if res[0][0] == 'tst':
                t1 = self.__parms['tn']
                t2 = self.__parms['rn']

            location = ''
            mA = re.match('0(\d+)', res[0][4])
            if mA:
                location += 'mA:' + mA.group()
            if re.match('0x', res[0][5]):
                location += ' mS:' + res[0][5]
            location += ' ' + res[0][6] + ' UTC'

            template.update({'text':'{} {} to {} {}\ndiff={} seconds\n{}'.format(t1,res[0][1],t2,res[0][3],res[0][7], location)})
            [h, m, s, ms] = list(map(lambda x: int(x), re.split(':|\.', res[0][6])))
            template.update({'time':hms2sec(h,m,s,ms)})
            template.update({'time_window':float(res[0][7])})
            #template['filters']['Time of Day'].update({'Time of Day Minimum':})
            #template['filters']['Time of Day'].update({'Time of Day Maximum':})
            template['filters']['Tracker Track Number'].update({t1 + ' track_num':res[0][1]})
            template['filters']['Tracker Track Number'].update({t2 + ' track_num':res[0][3]})

        self.__dct['view_points'].append(template)
        self.__id += 1 # special case => not obtained from 'hlp' file!...

    def track_deviation(self):
        res = track_deviation.findall(self.__deviation)
        if res:
            parms = {
                'ttN' : res[0][0],
                'tltN' : res[0][1],
                'rtN' : res[0][2],
                'rltN' : res[0][3],
                'h' : int(res[0][4]),
                'm' : int(res[0][5]),
                's' : int(res[0][6]),
                'ms' : int(res[0][7]),
            }
            if res[0][8] == 'reported':
                rep = track_reported.findall(self.__deviation)
                if rep and rep[0][0] in aoc_reported_deviation:
                    parms.update({'rk':rep[0][0],'rv1':rep[0][1],'rv2':rep[0][2]})
                    aoc_reported_deviation[rep[0][0]](self, parms)
                else:
                    print('missing (reported) token definition contents:',self.__deviation)
            elif res[0][8] == 'unexpected':
                rep = track_unexpected.findall(self.__deviation)
                if rep:
                    for tok in aoc_unexpected_deviation.keys():
                        if re.match(tok, rep[0]):
                            parms.update({'unxp':rep[0]})
                            aoc_unexpected_deviation[tok](self, parms)
                            break
                    else:
                        print('missing (unexpected) token definition:', rep[0],'contents:',self.__deviation)

    #----------------------------------------------------------------------------------
    # reported deviations
    #----------------------------------------------------------------------------------

    def track_mof_deviation(self,parms):
        template = {
            "id": self.__devpar['id'],
            "type": "MOF Content Deviation",
            "name": "MOF DEV #" + self.__devpar['nr'],
            "text": None,
            "time": None,
            "time_window": None,
            "db_objects": [
                "Tracker"
            ],
            "filters": {
                "Time of Day": {
                    "Time of Day Minimum": None,
                    "Time of Day Maximum": None
                },
                "Tracker Track Number": {
                }
            },
            "deviations": [
                "{}".format(self.__devpar['nr'])
            ],
            "context_variables": {
                "Tracker": [
                    "mof_long",
                    "mof_trans",
                    "mof_vert"
                ]
            }
        }

        template.update({'text':'{} {}:({}.{}) to {} {}:({}.{})\nreported {}={}; expected {}'.format(self.__devpar['tn'],parms['ttN'],parms['ttN'],parms['tltN'],self.__devpar['rn'],parms['rtN'],parms['rtN'],parms['rltN'],parms['rk'],parms['rv1'],parms['rv2'])})
        template.update({'time':hms2sec(parms['h'],parms['m'],parms['s'],parms['ms'])})
        template['filters']['Tracker Track Number'].update({self.__devpar['rn'] + ' track_num':parms['rtN']})
        template['filters']['Tracker Track Number'].update({self.__devpar['tn'] + ' track_num':parms['ttN']})

        times = get_times.findall(self.__deviation)
        if times:
            times.sort()
            template['filters']['Time of Day'].update({'Time of Day Minimum':times[0]})
            template['filters']['Time of Day'].update({'Time of Day Maximum':times[-1]})
            template.update({'time_window':time_delta(times[0],times[-1])})

        self.__dct['view_points'].append(template)

    def track_rocd_deviation(self,parms):
        template = {
            "id": self.__devpar['id'],
            "type": "ROCD Content Deviation",
            "name": "ROCD DEV #" + self.__devpar['nr'],
            "text": None,
            "time": None,
            "time_window": None,
            "db_objects": [
                "Tracker"
            ],
            "filters": {
                "Time of Day": {
                    "Time of Day Minimum": None,
                    "Time of Day Maximum": None
                },
                "Tracker Track Number": {
                }
            },
            "deviations": [
                "{}".format(self.__devpar['nr'])
            ],
            "context_variables": {
                "Tracker": [
                    "calc_vertical_rate_ftm"
                ]
            }
        }

        template.update({'text':'{} {}:({}.{}) to {} {}:({}.{})\nreported {}={}; expected {}'.format(self.__devpar['tn'],parms['ttN'],parms['ttN'],parms['tltN'],self.__devpar['rn'],parms['rtN'],parms['rtN'],parms['rltN'],parms['rk'],parms['rv1'],parms['rv2'])})
        template.update({'time':hms2sec(parms['h'],parms['m'],parms['s'],parms['ms'])})
        template['filters']['Tracker Track Number'].update({self.__devpar['rn'] + ' track_num':parms['rtN']})
        template['filters']['Tracker Track Number'].update({self.__devpar['tn'] + ' track_num':parms['ttN']})

        times = get_times.findall(self.__deviation)
        if times:
            times.sort()
            template['filters']['Time of Day'].update({'Time of Day Minimum':times[0]})
            template['filters']['Time of Day'].update({'Time of Day Maximum':times[-1]})
            template.update({'time_window':time_delta(times[0],times[-1])})

        self.__dct['view_points'].append(template)

    def track_tmc_deviation(self,parms):
        template = {
            "id": self.__devpar['id'],
            "type": "TMC Content Deviation",
            "name": "TMC DEV #" + self.__devpar['nr'],
            "text": None,
            "time": None,
            "time_window": None,
            "db_objects": [
                "Tracker"
            ],
            "filters": {
                "Time of Day": {
                    "Time of Day Minimum": None,
                    "Time of Day Maximum": None
                },
                "Tracker Track Number": {
                }
            },
            "deviations": [
                "{}".format(self.__devpar['nr'])
            ],
            "context_variables": {
                "Tracker": [
                    "modec_code_ft"
                ]
            }
        }

        template.update({'text':'{} {}:({}.{}) to {} {}:({}.{})\nreported {}={}; expected {}'.format(self.__devpar['tn'],parms['ttN'],parms['ttN'],parms['tltN'],self.__devpar['rn'],parms['rtN'],parms['rtN'],parms['rltN'],parms['rk'],parms['rv1'],parms['rv2'])})
        template.update({'time':hms2sec(parms['h'],parms['m'],parms['s'],parms['ms'])})
        template['filters']['Tracker Track Number'].update({self.__devpar['rn'] + ' track_num':parms['rtN']})
        template['filters']['Tracker Track Number'].update({self.__devpar['tn'] + ' track_num':parms['ttN']})

        times = get_times.findall(self.__deviation)
        if times:
            times.sort()
            template['filters']['Time of Day'].update({'Time of Day Minimum':times[0]})
            template['filters']['Time of Day'].update({'Time of Day Maximum':times[-1]})
            template.update({'time_window':time_delta(times[0],times[-1])})

        self.__dct['view_points'].append(template)

    def track_position_deviation(self,parms):
        template = {
            "id": self.__devpar['id'],
            "type": "POS Content Deviation",
            "name": "POS DEV #" + self.__devpar['nr'],
            "text": None,
            "time": None,
            "time_window": None,
            "db_objects": [
                "Tracker"
            ],
            "filters": {
                "Time of Day": {
                    "Time of Day Minimum": None,
                    "Time of Day Maximum": None
                },
                "Tracker Track Number": {
                }
            },
            "deviations": [
                "{}".format(self.__devpar['nr'])
            ],
        }

        template.update({'text':'{} {}:({}.{}) to {} {}:({}.{})\nreported {}={}; expected {}'.format(self.__devpar['tn'],parms['ttN'],parms['ttN'],parms['tltN'],self.__devpar['rn'],parms['rtN'],parms['rtN'],parms['rltN'],parms['rk'],parms['rv1'],parms['rv2'])})
        template.update({'time':hms2sec(parms['h'],parms['m'],parms['s'],parms['ms'])})
        template['filters']['Tracker Track Number'].update({self.__devpar['rn'] + ' track_num':parms['rtN']})
        template['filters']['Tracker Track Number'].update({self.__devpar['tn'] + ' track_num':parms['ttN']})

        times = get_times.findall(self.__deviation)
        if times:
            times.sort()
            template['filters']['Time of Day'].update({'Time of Day Minimum':times[0]})
            template['filters']['Time of Day'].update({'Time of Day Maximum':times[-1]})
            template.update({'time_window':time_delta(times[0],times[-1])})

        self.__dct['view_points'].append(template)

    def track_velocity_deviation(self,parms):
        template = {
            "id": self.__devpar['id'],
            "type": "VEL Content Deviation",
            "name": "VEL DEV #" + self.__devpar['nr'],
            "text": None,
            "time": None,
            "time_window": None,
            "db_objects": [
                "Tracker"
            ],
            "filters": {
                "Time of Day": {
                    "Time of Day Minimum": None,
                    "Time of Day Maximum": None
                },
                "Tracker Track Number": {
                }
            },
            "deviations": [
                "{}".format(self.__devpar['nr'])
            ],
            "context_variables": {
                "Tracker": [
                    "groundspeed_kt"
                ]
            }
        }

        template.update({'text':'{} {}:({}.{}) to {} {}:({}.{})\nreported {}={}; expected {}'.format(self.__devpar['tn'],parms['ttN'],parms['ttN'],parms['tltN'],self.__devpar['rn'],parms['rtN'],parms['rtN'],parms['rltN'],parms['rk'],parms['rv1'],parms['rv2'])})
        template.update({'time':hms2sec(parms['h'],parms['m'],parms['s'],parms['ms'])})
        template['filters']['Tracker Track Number'].update({self.__devpar['rn'] + ' track_num':parms['rtN']})
        template['filters']['Tracker Track Number'].update({self.__devpar['tn'] + ' track_num':parms['ttN']})

        times = get_times.findall(self.__deviation)
        if times:
            times.sort()
            template['filters']['Time of Day'].update({'Time of Day Minimum':times[0]})
            template['filters']['Time of Day'].update({'Time of Day Maximum':times[-1]})
            template.update({'time_window':time_delta(times[0],times[-1])})

        self.__dct['view_points'].append(template)

    def track_wgs_deviation(self,parms):
        template = {
            "id": self.__devpar['id'],
            "type": "WGS Content Deviation",
            "name": "WGS DEV #" + self.__devpar['nr'],
            "text": None,
            "time": None,
            "time_window": None,
            "db_objects": [
                "Tracker"
            ],
            "filters": {
                "Time of Day": {
                    "Time of Day Minimum": None,
                    "Time of Day Maximum": None
                },
                "Tracker Track Number": {
                }
            },
            "deviations": [
                "{}".format(self.__devpar['nr'])
            ],
            "context_variables": {
                "Meta": [
                    "pos_lat_deg",
                    "pos_long_deg"
                ]
            }
        }

        template.update({'text':'{} {}:({}.{}) to {} {}:({}.{})\nreported {}={}; expected {}'.format(self.__devpar['tn'],parms['ttN'],parms['ttN'],parms['tltN'],self.__devpar['rn'],parms['rtN'],parms['rtN'],parms['rltN'],parms['rk'],parms['rv1'],parms['rv2'])})
        template.update({'time':hms2sec(parms['h'],parms['m'],parms['s'],parms['ms'])})
        template['filters']['Tracker Track Number'].update({self.__devpar['rn'] + ' track_num':parms['rtN']})
        template['filters']['Tracker Track Number'].update({self.__devpar['tn'] + ' track_num':parms['ttN']})

        times = get_times.findall(self.__deviation)
        if times:
            times.sort()
            template['filters']['Time of Day'].update({'Time of Day Minimum':times[0]})
            template['filters']['Time of Day'].update({'Time of Day Maximum':times[-1]})
            template.update({'time_window':time_delta(times[0],times[-1])})

        self.__dct['view_points'].append(template)

    def track_acceleration_deviation(self,parms):
        template = {
            "id": self.__devpar['id'],
            "type": "ACC Content Deviation",
            "name": "ACC DEV #" + self.__devpar['nr'],
            "text": None,
            "time": None,
            "time_window": None,
            "db_objects": [
                "Tracker"
            ],
            "filters": {
                "Time of Day": {
                    "Time of Day Minimum": None,
                    "Time of Day Maximum": None
                },
                "Tracker Track Number": {
                }
            },
            "deviations": [
                "{}".format(self.__devpar['nr'])
            ],
            "context_variables": {
                "Meta": [
                    "accel_ax_ms2",
                    "accel_ay_ms2"
                ]
            }
        }

        template.update({'text':'{} {}:({}.{}) to {} {}:({}.{})\nreported {}={}; expected {}'.format(self.__devpar['tn'],parms['ttN'],parms['ttN'],parms['tltN'],self.__devpar['rn'],parms['rtN'],parms['rtN'],parms['rltN'],parms['rk'],parms['rv1'],parms['rv2'])})
        template.update({'time':hms2sec(parms['h'],parms['m'],parms['s'],parms['ms'])})
        template['filters']['Tracker Track Number'].update({self.__devpar['rn'] + ' track_num':parms['rtN']})
        template['filters']['Tracker Track Number'].update({self.__devpar['tn'] + ' track_num':parms['ttN']})

        times = get_times.findall(self.__deviation)
        if times:
            times.sort()
            template['filters']['Time of Day'].update({'Time of Day Minimum':times[0]})
            template['filters']['Time of Day'].update({'Time of Day Maximum':times[-1]})
            template.update({'time_window':time_delta(times[0],times[-1])})

        self.__dct['view_points'].append(template)

    def track_baro_altitude_deviation(self,parms):
        template = {
            "id": self.__devpar['id'],
            "type": "BH Content Deviation",
            "name": "BH DEV #" + self.__devpar['nr'],
            "text": None,
            "time": None,
            "time_window": None,
            "db_objects": [
                "Tracker"
            ],
            "filters": {
                "Time of Day": {
                    "Time of Day Minimum": None,
                    "Time of Day Maximum": None
                },
                "Tracker Track Number": {
                }
            },
            "deviations": [
                "{}".format(self.__devpar['nr'])
            ],
            "context_variables": {
                "Meta": [
                    "alt_baro_ft"
                ]
            }
        }

        template.update({'text':'{} {}:({}.{}) to {} {}:({}.{})\nreported {}={}; expected {}'.format(self.__devpar['tn'],parms['ttN'],parms['ttN'],parms['tltN'],self.__devpar['rn'],parms['rtN'],parms['rtN'],parms['rltN'],parms['rk'],parms['rv1'],parms['rv2'])})
        template.update({'time':hms2sec(parms['h'],parms['m'],parms['s'],parms['ms'])})
        template['filters']['Tracker Track Number'].update({self.__devpar['rn'] + ' track_num':parms['rtN']})
        template['filters']['Tracker Track Number'].update({self.__devpar['tn'] + ' track_num':parms['ttN']})

        times = get_times.findall(self.__deviation)
        if times:
            times.sort()
            template['filters']['Time of Day'].update({'Time of Day Minimum':times[0]})
            template['filters']['Time of Day'].update({'Time of Day Maximum':times[-1]})
            template.update({'time_window':time_delta(times[0],times[-1])})

        self.__dct['view_points'].append(template)

    def track_geom_altitude_deviation(self,parms):
        template = {
            "id": self.__devpar['id'],
            "type": "GH Content Deviation",
            "name": "GH DEV #" + self.__devpar['nr'],
            "text": None,
            "time": None,
            "time_window": None,
            "db_objects": [
                "Tracker"
            ],
            "filters": {
                "Time of Day": {
                    "Time of Day Minimum": None,
                    "Time of Day Maximum": None
                },
                "Tracker Track Number": {
                }
            },
            "deviations": [
                "{}".format(self.__devpar['nr'])
            ],
            "context_variables": {
                "Meta": [
                    "alt_geo_ft"
                ]
            }
        }

        template.update({'text':'{} {}:({}.{}) to {} {}:({}.{})\nreported {}={}; expected {}'.format(self.__devpar['tn'],parms['ttN'],parms['ttN'],parms['tltN'],self.__devpar['rn'],parms['rtN'],parms['rtN'],parms['rltN'],parms['rk'],parms['rv1'],parms['rv2'])})
        template.update({'time':hms2sec(parms['h'],parms['m'],parms['s'],parms['ms'])})
        template['filters']['Tracker Track Number'].update({self.__devpar['rn'] + ' track_num':parms['rtN']})
        template['filters']['Tracker Track Number'].update({self.__devpar['tn'] + ' track_num':parms['ttN']})

        times = get_times.findall(self.__deviation)
        if times:
            times.sort()
            template['filters']['Time of Day'].update({'Time of Day Minimum':times[0]})
            template['filters']['Time of Day'].update({'Time of Day Maximum':times[-1]})
            template.update({'time_window':time_delta(times[0],times[-1])})

        self.__dct['view_points'].append(template)

    def track_heading_deviation(self,parms):
        template = {
            "id": self.__devpar['id'],
            "type": "HDG Content Deviation",
            "name": "HDG DEV #" + self.__devpar['nr'],
            "text": None,
            "time": None,
            "time_window": None,
            "db_objects": [
                "Tracker"
            ],
            "filters": {
                "Time of Day": {
                    "Time of Day Minimum": None,
                    "Time of Day Maximum": None
                },
                "Tracker Track Number": {
                }
            },
            "deviations": [
                "{}".format(self.__devpar['nr'])
            ],
            "context_variables": {
                "Meta": [
                    "heading_deg"
                ]
            }
        }

        template.update({'text':'{} {}:({}.{}) to {} {}:({}.{})\nreported {}={}; expected {}'.format(self.__devpar['tn'],parms['ttN'],parms['ttN'],parms['tltN'],self.__devpar['rn'],parms['rtN'],parms['rtN'],parms['rltN'],parms['rk'],parms['rv1'],parms['rv2'])})
        template.update({'time':hms2sec(parms['h'],parms['m'],parms['s'],parms['ms'])})
        template['filters']['Tracker Track Number'].update({self.__devpar['rn'] + ' track_num':parms['rtN']})
        template['filters']['Tracker Track Number'].update({self.__devpar['tn'] + ' track_num':parms['ttN']})

        times = get_times.findall(self.__deviation)
        if times:
            times.sort()
            template['filters']['Time of Day'].update({'Time of Day Minimum':times[0]})
            template['filters']['Time of Day'].update({'Time of Day Maximum':times[-1]})
            template.update({'time_window':time_delta(times[0],times[-1])})

        self.__dct['view_points'].append(template)

    def track_status_deviation(self,parms):
        template = {
            "id": self.__devpar['id'],
            "type": "STA Content Deviation",
            "name": "STA DEV #" + self.__devpar['nr'],
            "text": None,
            "time": None,
            "time_window": None,
            "db_objects": [
                "Tracker"
            ],
            "filters": {
                "Time of Day": {
                    "Time of Day Minimum": None,
                    "Time of Day Maximum": None
                },
                "Tracker Track Number": {
                }
            },
            "deviations": [
                "{}".format(self.__devpar['nr'])
            ]
        }

        template.update({'text':'{} {}:({}.{}) to {} {}:({}.{})\nreported {}={}; expected {}'.format(self.__devpar['tn'],parms['ttN'],parms['ttN'],parms['tltN'],self.__devpar['rn'],parms['rtN'],parms['rtN'],parms['rltN'],parms['rk'],parms['rv1'],parms['rv2'])})
        template.update({'time':hms2sec(parms['h'],parms['m'],parms['s'],parms['ms'])})
        template['filters']['Tracker Track Number'].update({self.__devpar['rn'] + ' track_num':parms['rtN']})
        template['filters']['Tracker Track Number'].update({self.__devpar['tn'] + ' track_num':parms['ttN']})

        times = get_times.findall(self.__deviation)
        if times:
            times.sort()
            template['filters']['Time of Day'].update({'Time of Day Minimum':times[0]})
            template['filters']['Time of Day'].update({'Time of Day Maximum':times[-1]})
            template.update({'time_window':time_delta(times[0],times[-1])})

        self.__dct['view_points'].append(template)

    #----------------------------------------------------------------------------------
    # unexpected deviations
    #----------------------------------------------------------------------------------

    def track_m3a_deviation(self,parms):
        template = {
            "id": self.__devpar['id'],
            "type": "M3A Content Deviation",
            "name": "M3A DEV #" + self.__devpar['nr'],
            "text": None,
            "time": None,
            "time_window": None,
            "db_objects": [
                "Tracker"
            ],
            "filters": {
                "Time of Day": {
                    "Time of Day Minimum": None,
                    "Time of Day Maximum": None
                },
                "Tracker Track Number": {
                }
            },
            "deviations": [
                "{}".format(self.__devpar['nr'])
            ],
            "context_variables": {
                "Tracker": [
                    "mode3a_code"
                ]
            }
        }

        template.update({'text':'{} {}:({}.{}) to {} {}:({}.{})\nunexpected {}'.format(self.__devpar['tn'],parms['ttN'],parms['ttN'],parms['tltN'],self.__devpar['rn'],parms['rtN'],parms['rtN'],parms['rltN'],parms['unxp'])})
        template.update({'time':hms2sec(parms['h'],parms['m'],parms['s'],parms['ms'])})
        template['filters']['Tracker Track Number'].update({self.__devpar['rn'] + ' track_num':parms['rtN']})
        template['filters']['Tracker Track Number'].update({self.__devpar['tn'] + ' track_num':parms['ttN']})

        times = get_times.findall(self.__deviation)
        if times:
            times.sort()
            template['filters']['Time of Day'].update({'Time of Day Minimum':times[0]})
            template['filters']['Time of Day'].update({'Time of Day Maximum':times[-1]})
            template.update({'time_window':time_delta(times[0],times[-1])})

        self.__dct['view_points'].append(template)

    def track_m2_deviation(self,parms):
        template = {
            "id": self.__devpar['id'],
            "type": "M2 Content Deviation",
            "name": "M2 DEV #" + self.__devpar['nr'],
            "text": None,
            "time": None,
            "time_window": None,
            "db_objects": [
                "Tracker"
            ],
            "filters": {
                "Time of Day": {
                    "Time of Day Minimum": None,
                    "Time of Day Maximum": None
                },
                "Tracker Track Number": {
                }
            },
            "deviations": [
                "{}".format(self.__devpar['nr'])
            ],
            "context_variables": {
                "Tracker": [
                    "mode2_code"
                ]
            }
        }

        template.update({'text':'{} {}:({}.{}) to {} {}:({}.{})\nunexpected {}'.format(self.__devpar['tn'],parms['ttN'],parms['ttN'],parms['tltN'],self.__devpar['rn'],parms['rtN'],parms['rtN'],parms['rltN'],parms['unxp'])})
        template.update({'time':hms2sec(parms['h'],parms['m'],parms['s'],parms['ms'])})
        template['filters']['Tracker Track Number'].update({self.__devpar['rn'] + ' track_num':parms['rtN']})
        template['filters']['Tracker Track Number'].update({self.__devpar['tn'] + ' track_num':parms['ttN']})

        times = get_times.findall(self.__deviation)
        if times:
            times.sort()
            template['filters']['Time of Day'].update({'Time of Day Minimum':times[0]})
            template['filters']['Time of Day'].update({'Time of Day Maximum':times[-1]})
            template.update({'time_window':time_delta(times[0],times[-1])})

        self.__dct['view_points'].append(template)

#----------------------------------------------------------------------------------
# dispatch tables
#----------------------------------------------------------------------------------

# used for 'hlp' file
aoc_tokens_hlp = {
    'AOC' : aoc_parser.aoc_version,
    'DEV' : aoc_parser.start_deviation,
    'DT ' : aoc_parser.delta_time,
    'NOW' : aoc_parser.time_now,
    'REF' : aoc_parser.fname_ref,
    'TST' : aoc_parser.fname_tst,
}

# used for 'out' file
aoc_tokens_out = {
    'Loading reference run' : 'ref',
    'Loading test run' : 'tst',
    'Pass 1: Establish common time-frame' : 'pass1',
    'Pass 2: Correlate system tracks' : 'pass2',
    'Pass 3: Compare system track contents' : 'pass3',
    'Server Identification:' : aoc_parser.sac_sic,
    'Time of Updates:' : aoc_parser.time,
    'Duration:' : aoc_parser.duration,
    'Deviation: Short' : aoc_parser.short_track, # special case not found in 'hlp' file
}

# general deviation tokens
aoc_tokens_deviation = {
    'Extra test' : aoc_parser.extra_track,
    'Missing reference' : aoc_parser.missing_track,
    'Track contents' : aoc_parser.track_deviation,
}

# specialized (reported) deviations
aoc_reported_deviation = {
    'mof_longi' : aoc_parser.track_mof_deviation,
    'mof_trans' : aoc_parser.track_mof_deviation,
    'mof_verti' : aoc_parser.track_mof_deviation,
    'rocd' : aoc_parser.track_rocd_deviation,
    'tmc' : aoc_parser.track_tmc_deviation,
    'x_position' : aoc_parser.track_position_deviation,
    'y_position' : aoc_parser.track_position_deviation,
    'x_velocity' : aoc_parser.track_velocity_deviation,
    'y_velocity' : aoc_parser.track_velocity_deviation,
    'longitude' : aoc_parser.track_wgs_deviation,
    'latitude' : aoc_parser.track_wgs_deviation,
    'x_acceleration' : aoc_parser.track_acceleration_deviation,
    'y_acceleration' : aoc_parser.track_acceleration_deviation,
    'baro_altitude' : aoc_parser.track_baro_altitude_deviation,
    'geom_altitude' : aoc_parser.track_geom_altitude_deviation,
    'geom_altitude' : aoc_parser.track_heading_deviation,
    'status.aac' : aoc_parser.track_status_deviation,
    'status.aff' : aoc_parser.track_status_deviation,
    'status.amalgamated' : aoc_parser.track_status_deviation,
    'status.coasted' : aoc_parser.track_status_deviation,
    'status.combined' : aoc_parser.track_status_deviation,
    'status.cor' : aoc_parser.track_status_deviation,
    'status.cst_ads' : aoc_parser.track_status_deviation,
    'status.cst_mds' : aoc_parser.track_status_deviation,
    'status.cst_psr' : aoc_parser.track_status_deviation,
    'status.cst_ssr' : aoc_parser.track_status_deviation,
    'status.cst' : aoc_parser.track_status_deviation,
    'status.emergency' : aoc_parser.track_status_deviation,
    'status.flightplan' : aoc_parser.track_status_deviation,
    'status.formation' : aoc_parser.track_status_deviation,
    'status.ghost_track' : aoc_parser.track_status_deviation,
    'status.manoeuvring' : aoc_parser.track_status_deviation,
    'status.md4' : aoc_parser.track_status_deviation,
    'status.md5' : aoc_parser.track_status_deviation,
    'status.military_emergency' : aoc_parser.track_status_deviation,
    'status.military_ident' : aoc_parser.track_status_deviation,
    'status.mrh' : aoc_parser.track_status_deviation,
    'status.multisensor_track' : aoc_parser.track_status_deviation,
    'status.no_mode_a' : aoc_parser.track_status_deviation,
    'status.number_extends' : aoc_parser.track_status_deviation,
    'status.psr_only' : aoc_parser.track_status_deviation,
    'status.simulated_track' : aoc_parser.track_status_deviation,
    'status.spi' : aoc_parser.track_status_deviation,
    'status.ssr_only' : aoc_parser.track_status_deviation,
    'status.stp' : aoc_parser.track_status_deviation,
    'status.suc' : aoc_parser.track_status_deviation,
    'status.surface_target' : aoc_parser.track_status_deviation,
    'status.tdc' : aoc_parser.track_status_deviation,
    'status.tentative_track' : aoc_parser.track_status_deviation,
    'status.track_type' : aoc_parser.track_status_deviation,
    'status.updated_by_add' : aoc_parser.track_status_deviation,
}

# specialized (unexpected) deviations
aoc_unexpected_deviation = {
    'SSR mode 3/A code' : aoc_parser.track_m3a_deviation,
    'SSR mode 2 code' : aoc_parser.track_m2_deviation,
}

#----------------------------------------------------------------------------------
# main
#----------------------------------------------------------------------------------

def main(argv):
    parser = argparse.ArgumentParser(description='COMPASS view points generator given AOC (ARTAS Output Comparator) results')
    parser.add_argument('-ah', help='AOC helper filename (*.hlp)', required=True)
    parser.add_argument('-ao', help='AOC output filename (*.out)', required=True)
    parser.add_argument('-o', help='view points filename (default: aoc_view_points.json)', default='aoc_view_points.json')
    parser.add_argument('-sd', help='start date', default= '')
    parser.add_argument('-r', help='reference tracker name (default: REF)', default= 'REF')
    parser.add_argument('-t', help='test tracker name (default: TST)', default= 'TST')
    args = parser.parse_args()

    # default attributes used
    viewpoints = {
        'view_point_context': {
            'version' : '0.1',
            'aoc_version' : None,
            'start_date' : None,
            'time_now' : None,
            'common_time_duration': None,
            'datasets' : [
                {
                    'name' : 'reference_run',
                    'ds_name' : None,
                    'filename': None,
                    'ds_sac': None,
                    'ds_sic': None,
                    'time_start': None,
                    'time_end': None,
                    'duration': None,
                },
                {
                    'name' : 'test_run',
                    'ds_name' : None,
                    'filename': None,
                    'time_offset': None,
                    'time_offset_stddev': None,
                    'ds_sac': None,
                    'ds_sic': None,
                    'time_start': None,
                    'time_end': None,
                    'duration': None,
                }
            ]
        },
        'view_points' : []
    }

    if args.sd == '':
        del(viewpoints['view_point_context']['start_date'])
    else:
        viewpoints['view_point_context'].update({'start_date' : args.sd})

    viewpoints['view_point_context']['datasets'][0].update({'ds_name' : args.r})
    viewpoints['view_point_context']['datasets'][1].update({'ds_name' : args.t})

    parms = {
        'hlp' : args.ah,
        'out' : args.ao,
        'rn' : args.r,
        'tn' : args.t,
    }

    aoc = aoc_parser(parms, viewpoints)

    with open(args.o, 'w') as vp_file:
        json.dump(aoc.viewpoints(), vp_file, indent=4)

if __name__ == "__main__":
    main(sys.argv[1:])
