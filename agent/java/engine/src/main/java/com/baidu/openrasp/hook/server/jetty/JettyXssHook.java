package com.baidu.openrasp.hook.server.jetty;

import com.baidu.openrasp.HookHandler;
import com.baidu.openrasp.hook.AbstractClassHook;
import com.baidu.openrasp.hook.server.ServerXssHook;
import com.baidu.openrasp.plugin.checker.CheckParameter;
import com.baidu.openrasp.tool.Reflection;
import com.baidu.openrasp.tool.hook.ServerXss;
import javassist.CannotCompileException;
import javassist.CtClass;
import javassist.NotFoundException;

import java.io.IOException;
import java.util.HashMap;

/**
 * @author anyang
 * @Description: jetty 获取输出buffer的hook点
 * @date 2018/8/1315:13
 */
public class JettyXssHook extends ServerXssHook {

    @Override
    public boolean isClassMatched(String className) {
        return "org/eclipse/jetty/server/AbstractHttpConnection".equals(className);
    }

    @Override
    protected void hookMethod(CtClass ctClass) throws IOException, CannotCompileException, NotFoundException {

        String src = getInvokeStaticSrc(JettyXssHook.class, "getJettyOutputBuffer", "_generator", Object.class);
        insertBefore(ctClass, "completeResponse", "()V", src);

    }


    public static void getJettyOutputBuffer(Object object) {

        try {
            Object buffer = Reflection.getSuperField(object, "_buffer");
            String content = new String(buffer.toString().getBytes(), "utf-8");
            HashMap<String, Object> params = ServerXss.generateXssParameters(content);
            HookHandler.doCheck(CheckParameter.Type.XSS, params);

        } catch (Exception e) {

            e.printStackTrace();
        }

    }
}