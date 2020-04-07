/usr/local/neuware/samples/cncodec/cncodec/bin/decode --chan-num 1 \
                                                      --chan -d 0 -i 1080p.h264 -c H264 -f NV12 --test-round 100
exit
/usr/local/neuware/samples/cncodec/cncodec/bin/jdecode  --chan-num 6 --chan -d 0 -i 1920x1080.jpg --test-round 200 \
                                                        --chan -d 1 -i 1920x1080.jpg --test-round 200 \
                                                        --chan -d 2 -i 1920x1080.jpg --test-round 200 \
                                                        --chan -d 3 -i 1920x1080.jpg --test-round 200 \
                                                        --chan -d 4 -i 1920x1080.jpg --test-round 200 \
                                                        --chan -d 5 -i 1920x1080.jpg --test-round 200
#decode
