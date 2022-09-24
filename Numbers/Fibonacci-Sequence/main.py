

def fibonacci(mode, num):
    a, b = 1, 1
    print('1 1', end=' ')
    if mode == 1:
        # 代表序列到num为止
        while b < num:
            tmp = a + b
            a = b
            b = tmp
            print(b, end=' ')
    elif mode == 2:
        # 代表序列到第num个为止
        for i in range(2, num):
            tmp = a + b
            a = b
            b = tmp
            print(b, end=' ')
    

if __name__ == '__main__':
    mode = input('Please select mode, 1(to a specific number) 2(to the Nth number): ')
    num = input('Please input your number: ')
    fibonacci(int(mode), int(num))
