```c
typedef struct{

    short temp;
    short humi;

}weather_data_t;

weather_data_t china_shenzhen;

weather_data_t trans_data(weather_data_t *locate)
{
    /*add mutex to protec data*/

    static weather_data_t s_locate={
        .humi = 0,
        .temp = 0
    };

    if ( locate ){
        
        /*write*/
        memcpy( &s_locate,
                locate,
                sizeof(weather_data_t)
        );

    }else{
        /*read*/
        return s_locate;
    }

    return 0;

}

void thread_set_shenzhen_info(void)
{
    weather_data_t s_locate = {
        .humi = 0,
        .temp = 0
    };

    s_locate.humi = 45;
    s_locate.temp = 25;

    trans_data(&s_locate);
    
}


void thread_get_shenzhen_info(void)
{
    weather_data_t s_locate = {
        .humi = 0,
        .temp = 0
    };

    s_locate = trans_data(NULL);

    printf(" shenzhen-temp:%d,humi:%d \r\n",
            s_locate.temp,
            s_locate.humi
    );

}
```
