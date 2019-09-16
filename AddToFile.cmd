/** AddToFile.cmd
 * Adds the specified line to the end of the specified file.
 * Written by and Copyright (c) 2010-2017 David Azarewicz http://88watts.net
 *
 * @#D Azarewicz:1.02#@##1## 02 Jun 2017              DAZAR1    ::::::@@AddToFile.cmd (c) David Azarewicz 2017
 * V1.01 16-Sep-2016 First official release
 * V1.02 02-Jun-2017 Added Asd to bldlevel, added DATE1
 *
 * The following line is for the help sample code for the VAR function:
EXAMPLEVAR=Example String
 */
call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
call SysLoadFuncs
parse arg OutFile','String','Function','Parm1','Parm2','Parm3','Parm4','Parm5;

if (OutFile='') then do
  Say 'Usage:';
  Say '  AddToFile.cmd FileName,String,Function[,Parameters...]';
  Say 'Functions:';
  Say '  DATEL - Adds the date as a long number.';
  Say '  DATE1 - Adds the date in 2017-Jul-01 format.';
  Say '  BLDLEVEL - Adds a standardized formatted BLDLEVEL string.';
  Say '  DATEYEAR - Adds the current year.';
  Say '  DATEMONTH - Adds the current month.';
  Say '  DATEDAY - Adds the current day.';
  Say '  VERSIONMAJOR - Adds the major portion of the provided version number.';
  Say '  VERSIONMINOR - Adds the minor portion of the provided version number.';
  Say '  VERSIONREVISION - Adds the revision portion of the provided version number.';
  Say '  VAR - Adds the value of the specified variable from a specified file.';
  Say '  FILE - Adds the contents of a file.';
  Say 'Examples:';
  MyFile='AddToFile.tmp';
  rc=SysFileDelete(MyFile);
  MyCmd=MyFile||',#define DDATE,DATEL';
  rc=LineOut(MyFile, '--- AddToFile.cmd '||MyCmd);
  call 'AddToFile.cmd' MyCmd;
  MyCmd=MyFile||',#define DDATE,DATE1';
  rc=LineOut(MyFile, '--- AddToFile.cmd '||MyCmd);
  call 'AddToFile.cmd' MyCmd;
  MyCmd=MyFile||',option description,BLDLEVEL,Vendor,1.2.3,Description,Fixpack,Asd';
  rc=LineOut(MyFile, '--- AddToFile.cmd '||MyCmd);
  call 'AddToFile.cmd' MyCmd;
  MyCmd=MyFile||',char *bl = "%A";,BLDLEVEL,Vendor,1.2.3,Description,Fixpack';
  rc=LineOut(MyFile, '--- AddToFile.cmd '||MyCmd);
  call 'AddToFile.cmd' MyCmd;
  MyCmd=MyFile||',#define DYEAR,DATEYEAR';
  rc=LineOut(MyFile, '--- AddToFile.cmd '||MyCmd);
  call 'AddToFile.cmd' MyCmd;
  MyCmd=MyFile||',#define DMONTH,DATEMONTH';
  rc=LineOut(MyFile, '--- AddToFile.cmd '||MyCmd);
  call 'AddToFile.cmd' MyCmd;
  MyCmd=MyFile||',#define DDAY,DATEDAY';
  rc=LineOut(MyFile, '--- AddToFile.cmd '||MyCmd);
  call 'AddToFile.cmd' MyCmd;
  MyCmd=MyFile||',#define DVMAJ,VERSIONMAJOR,1.2.3';
  rc=LineOut(MyFile, '--- AddToFile.cmd '||MyCmd);
  call 'AddToFile.cmd' MyCmd;
  MyCmd=MyFile||',#define DVMIN,VERSIONMINOR,1.2.3';
  rc=LineOut(MyFile, '--- AddToFile.cmd '||MyCmd);
  call 'AddToFile.cmd' MyCmd;
  MyCmd=MyFile||',#define DVREV,VERSIONREVISION,1.2.3';
  rc=LineOut(MyFile, '--- AddToFile.cmd '||MyCmd);
  call 'AddToFile.cmd' MyCmd;
  MyCmd=MyFile||',#define XYZ "%A",VAR,EXAMPLEVAR=,AddToFile.cmd';
  rc=LineOut(MyFile, '--- AddToFile.cmd '||MyCmd);
  call 'AddToFile.cmd' MyCmd;
  MyCmd=MyFile||',InFileName,FILE';
  rc=LineOut(MyFile, '--- AddToFile.cmd '||MyCmd);
  call 'AddToFile.cmd' MyCmd;
  do while LINES(MyFile) <> 0;
    Say LINEIN(MyFile);
  end;
  rc=stream(MyFile,'c','close');
  rc=SysFileDelete(MyFile);
  exit;
end

select
  when (Function='DATEL') then do
    NewStr=DATE('S')||'L'
    RepLoc=Pos('%A', String);
    if (RepLoc>0) then OutStr=Substr(String,1,RepLoc-1)||NewStr||Substr(String,RepLoc+2);
    else OutStr=String||' '||NewStr;
    rc=lineout(OutFile, OutStr);
    rc=lineout(OutFile);
  end

  when (LEFT(Function,8)='BLDLEVEL') then do
    Type=SUBSTR(Function,9,1);

    /* get hostname for build system */
    ProjHost = VALUE('HOSTNAME',,'OS2ENVIRONMENT');

    /* projString = projDate" "projTime"       "projHost */
    ProjString = left(Date()||' '||TIME(),25)||left(ProjHost,10);

    /* get fixpack from SVN version */
    if (Parm4='SVN') then Parm4='SVN'||SvnVersion();
    if (Parm2='DATE') then Parm2=SUBSTR(DATE('S'), 1, 4)||'.'||SUBSTR(DATE('S'), 5, 2)||'.'||SUBSTR(DATE('S'), 7, 2);

    parse var Parm2 major'.'minor'.'ProjVersion;

    RepLoc=Pos('%Y', Parm3);
    if (RepLoc>0) then do
      Parm3=Substr(Parm3,1,RepLoc-1)||FORMAT(SUBSTR(DATE('S'), 1, 4))||Substr(Parm3,RepLoc+2);
    end

    NewStr='@#'||Parm1||':'||Parm2||'#@##1## '||ProjString||':'||Parm5||':::'||ProjVersion||'::'||Parm4||'@@'||Parm3;
    if (Type='2') then do
      /*OutStr=String||' "@#'||Parm1||':'||Parm2||'#@##1## '||ProjString||'::::'||ProjVersion||'::'||Parm4||'@@'||Parm3||'"';*/
      OutStr=String||' "'||NewStr||'"';
    end
    else do
      RepLoc=Pos('%A', String);
      if (RepLoc>0) then OutStr=Substr(String,1,RepLoc-1)||NewStr||Substr(String,RepLoc+2);
      else OutStr=String||" '"||NewStr||"'";
    end
    rc=lineout(OutFile, OutStr);
    rc=lineout(OutFile);
  end

  when (Function='DATEYEAR') then do
    NewStr=FORMAT(SUBSTR(DATE('S'), 1, 4));
    RepLoc=Pos('%A', String);
    if (RepLoc>0) then OutStr=Substr(String,1,RepLoc-1)||NewStr||Substr(String,RepLoc+2);
    else OutStr=String||' '||NewStr;
    rc=lineout(OutFile, OutStr);
    rc=lineout(OutFile);
  end

  when (Function='DATEMONTH') then do
    NewStr=FORMAT(SUBSTR(DATE('S'), 5, 2));
    RepLoc=Pos('%A', String);
    if (RepLoc>0) then OutStr=Substr(String,1,RepLoc-1)||NewStr||Substr(String,RepLoc+2);
    else OutStr=String||' '||NewStr;
    rc=lineout(OutFile, OutStr);
    rc=lineout(OutFile);
  end

  when (Function='DATEDAY') then do
    NewStr=FORMAT(SUBSTR(DATE('S'), 7, 2));
    RepLoc=Pos('%A', String);
    if (RepLoc>0) then OutStr=Substr(String,1,RepLoc-1)||NewStr||Substr(String,RepLoc+2);
    else OutStr=String||' '||NewStr;
    rc=lineout(OutFile, OutStr);
    rc=lineout(OutFile);
  end

  when (Function='DATE1') then do
    parse value Date('N') with MyDay MyMonth MyYear;
    NewStr=MyYear||'-'||MyMonth||'-'||Right(MyDay, 2, 0);
    RepLoc=Pos('%A', String);
    if (RepLoc>0) then OutStr=Substr(String,1,RepLoc-1)||NewStr||Substr(String,RepLoc+2);
    else OutStr=String||' '||NewStr;
    rc=lineout(OutFile, OutStr);
    rc=lineout(OutFile);
  end

  when (Function="VERSIONMAJOR") then do
    parse var Parm1 Major'.'Minor'.'Revision;
    NewStr=FORMAT(Major);
    RepLoc=Pos('%A', String);
    if (RepLoc>0) then OutStr=Substr(String,1,RepLoc-1)||NewStr||Substr(String,RepLoc+2);
    else OutStr=String||' '||NewStr;
    rc=lineout(OutFile, OutStr);
    rc=lineout(OutFile);
  end

  when (Function="VERSIONMINOR") then do
    parse var Parm1 Major'.'Minor'.'Revision;
    NewStr=FORMAT(Minor);
    RepLoc=Pos('%A', String);
    if (RepLoc>0) then OutStr=Substr(String,1,RepLoc-1)||NewStr||Substr(String,RepLoc+2);
    else OutStr=String||' '||NewStr;
    rc=lineout(OutFile, OutStr);
    rc=lineout(OutFile);
  end

  when (Function="VERSIONREVISION") then do
    parse var Parm1 Major'.'Minor'.'Revision;
    NewStr=FORMAT(Revision);
    RepLoc=Pos('%A', String);
    if (RepLoc>0) then OutStr=Substr(String,1,RepLoc-1)||NewStr||Substr(String,RepLoc+2);
    else OutStr=String||' '||NewStr;
    rc=lineout(OutFile, OutStr);
    rc=lineout(OutFile);
  end

  when (Function="FILE") then do
    do while LINES(String) <> 0;
      rc=LINEOUT(OutFile, LINEIN(String));
    end;
    rc=stream(String,'c','close');
    rc=lineout(OutFile);
  end

  when (Function="VAR") then do
    Type=SUBSTR(Function,4,1);
    NewStr = '';
    Len=LENGTH(Parm1);
    do while LINES(Parm2) <> 0;
      Line1=LINEIN(Parm2);
      if (LEFT(Line1, Len) = Parm1) then do
        NewStr=SUBSTR(Line1, Len+1);
        LEAVE;
      end
    end;
    rc=stream(Parm2,'c','close');

    RepLoc=Pos('%A', String);
    if (RepLoc>0) then OutStr=Substr(String,1,RepLoc-1)||NewStr||Substr(String,RepLoc+2);
    else OutStr=String||' '||NewStr;
    rc=LINEOUT(OutFile, OutStr);
    rc=lineout(OutFile);
  end

  otherwise do
    say "AddToFile: Unrecognized command: "||Function;
    exit 1;
  end
end

exit 0;

SvnVersion:
  File = ARG(1);
  '@svn info 2>&1 | rxqueue'
  do while (QUEUED() > 0)
    PARSE PULL Line1':'Line2
    if (Line1 = "Last Changed Rev") then leave;
    Line2 = "";
  end
  do while (QUEUED() <> 0); PULL; end;
  Return(STRIP(Line2,'B'));

