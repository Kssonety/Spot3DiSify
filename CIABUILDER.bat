@echo off
bannertool.exe makebanner -i banner.png -a audio.wav -o banner.bnr
bannertool.exe makesmdh -s "Spot3DiSify" -l "Spot3DiSify" -p "Kssonety" -i icon.png  -o icon.icn
makerom -f cia -o Spot3DiSify.cia -DAPP_ENCRYPTED=false -rsf Spot3DiSify.rsf -target t -exefslogo -elf Spot3DiSify.elf -icon icon.icn -banner banner.bnr
makerom -f cci -o Spot3DiSify.3ds -DAPP_ENCRYPTED=true -rsf Spot3DiSify.rsf -target t -exefslogo -elf Spot3DiSify.elf -icon icon.icn -banner banner.bnr
echo Finished! 3DS and CIA have been built!