# gencodeTranscriptionSupportLevel.sql was originally generated by the autoSql program, which also 
# generated gencodeTranscriptionSupportLevel.c and gencodeTranscriptionSupportLevel.h.  This creates the database representation of
# an object which can be loaded and saved from RAM in a fairly 
# automatic way.

#GENCODE transcription support level, computed from primary data
CREATE TABLE gencodeTranscriptionSupportLevel (
    transcriptId varchar(255) not null,	# GENCODE transcript identifier
    level varchar(255) not null,	# support level, tsl1 is strongest support, tsl5 weakest, NA means not analyzed
              #Indices
    PRIMARY KEY(transcriptId)
);
