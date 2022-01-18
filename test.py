import cv2
import numpy as np

class Stable:
    # surf 特征提取
    __surf = {
        # surf算法
        'surf': None,
        # 提取的特征点
        'kp': None,
        # 描述符
        'des': None,
        # 过滤后的特征模板
        'template_kp': None
    }

    # 配置
    __config = {
        # 要保留的最佳特征的数量
        'key_point_count': 600,
        # Flann特征匹配
        'index_params': dict(algorithm=0, trees=5),
        'search_params': dict(checks=50),
        'ratio': 0.5,
    }

    # 特征提取列表
    __surf_list = []

    frameWidth = 0
    farmeHeight = 0

    def __init__(self):
        pass

    # 初始化surf
    def __init_surf(self):
        self.__surf['surf'] = cv2.xfeatures2d.SURF_create(self.__config['key_point_count'])

        first_frame = cv2.imread(r'C:/Users/7723/Desktop/codes/halcon/PicAndTemplate/V1001559/1_border/0.png')
        first_frame = cv2.cvtColor(first_frame, cv2.COLOR_BGR2GRAY)

        self.frameWidth = first_frame.shape[1]
        self.frameHeight = first_frame.shape[0]

        last_frame = cv2.imread(r'C:/Users/7723/Desktop/codes/halcon/PicAndTemplate/V1001559/1_border/10.png')
        last_frame = cv2.cvtColor(last_frame, cv2.COLOR_BGR2GRAY)

        self.__surf['kp'], self.__surf['des'] = self.__surf['surf'].detectAndCompute(first_frame, None)
        kp, des = self.__surf['surf'].detectAndCompute(last_frame, None)

        # 快速临近匹配
        flann = cv2.FlannBasedMatcher(self.__config['index_params'], self.__config['search_params'])
        matches = flann.knnMatch(self.__surf['des'], des, k=2)

        good_match = []
        for m, n in matches:
            if m.distance < self.__config['ratio'] * n.distance:
                good_match.append(m)

        self.__surf['template_kp'] = []
        for f in good_match:
            self.__surf['template_kp'].append(self.__surf['kp'][f.queryIdx])

    # 处理
    def __process(self):
        for i in range(0, 11):
            computeFrame = cv2.imread('C:/Users/7723/Desktop/codes/halcon/PicAndTemplate/V1001559/1_border/' + str(i) + '.png')
            print(i, end=' ')
            try:
                output_frame = self.detect_compute(computeFrame)
                cv2.imwrite('C:/Users/7723/Desktop/codes/halcon/PicAndTemplate/V1001559/2_SURF&FLANN/' + str(i) + '.png', output_frame)
                print('success')
            except:
                print('failed')
                continue

    # 视频稳像
    def stable(self):
        self.__init_surf()
        self.__process()

    # 特征点提取
    def detect_compute(self, computeFrame):

        frame_gray = cv2.cvtColor(computeFrame, cv2.COLOR_BGR2GRAY)

        # 计算特征点
        kp, des = self.__surf['surf'].detectAndCompute(frame_gray, None)

        # 快速临近匹配
        flann = cv2.FlannBasedMatcher(self.__config['index_params'], self.__config['search_params'])
        matches = flann.knnMatch(self.__surf['des'], des, k=2)

        # 计算单应性矩阵
        good_match = []
        for m, n in matches:
            if m.distance < self.__config['ratio'] * n.distance:
                good_match.append(m)

        # 特征模版过滤
        p1, p2 = [], []
        for f in good_match:
            if self.__surf['kp'][f.queryIdx] in self.__surf['template_kp']:
                p1.append(self.__surf['kp'][f.queryIdx].pt)
                p2.append(kp[f.trainIdx].pt)

        # 单应性矩阵
        H, _ = cv2.findHomography(np.float32(p2), np.float32(p1), cv2.RHO)

        # 透视变换
        output_frame = cv2.warpPerspective(computeFrame, H, (self.frameWidth, self.frameHeight), borderMode=cv2.BORDER_REPLICATE)

        return output_frame


if __name__ == '__main__':
    s = Stable()
    s.stable()

    print('[INFO] Done.')
    exit(0)
