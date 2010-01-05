#!python -w

class Rec:

	def __init__(self, line):
		arr = line.split('|')

		self.type = arr[0]
		self.path = arr[1]
		self.title = arr[2]
		self.genre = arr[3]

		if len(arr) > 4:
			self.lastplayed = int(arr[4])
			self.playcount = int(arr[5])

	def __cmp__(self, other):
		return cmp(self.title, other.title)

adds = []
dels = []

import sys

for line in sys.stdin:
	
	rec = Rec(line)
	if (rec.type == "add"):
		adds.append(rec)
	else:
		dels.append(rec)

adds.sort()
dels.sort()

for x in adds:
	found = None
	
	for j in dels:
		if j.title == x.title:
			found = j

	if found is not None:
		print "UPDATE video_files SET playcount = %(playcount)d, lastplayed = %(lastplayed)d WHERE path = '%(path)s';" % \
					{ "playcount" : found.playcount,
						"lastplayed": found.lastplayed,
						"path"      : x.path
						}

	
